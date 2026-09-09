#include "overlay.h"

#include "input.h"
#include "settings.h"

#include <coreinit/cache.h>
#include <gx2/registers.h>
#include <gx2/shaders.h>
#include <gx2/state.h>
#include <gx2/swap.h>
#include <malloc.h>
#include <math.h>
#include <string.h>
#include <vpad/input.h>

#include <atomic>

#include "imgui.h"
#include "imgui_impl_gx2.h"

namespace Mss {
namespace Overlay {
namespace {

constexpr float kLogicalWidth = 1920.0f;
constexpr float kLogicalHeight = 1080.0f;
constexpr float kWatermarkScale = 0.65f;
constexpr uint32_t kMenuToggle =
    VPAD_BUTTON_ZL | VPAD_BUTTON_L | VPAD_BUTTON_MINUS;

const RplHost* sHost = nullptr;

std::atomic<bool> sReady{false};
std::atomic<uint32_t> sSnapshotSequence{0};
std::atomic<uint32_t> sFrameSerial{0};

struct FrameSnapshot {
    RplPad pad;
    bool comboHeld;
    bool macroActive;
    bool plusHeld;
    bool stickDown;
};

FrameSnapshot sSnapshot = {};
std::atomic<bool> sMenuVisible{false};

GX2ContextState* sOverlayContext = nullptr;
GX2ContextState* sGameContext = nullptr;
bool sContextReady = false;
bool sImGuiReady = false;

uint32_t sBuiltFrameSerial = UINT32_MAX;

struct DrawnSurface {
    void* image;
    uint32_t width;
    uint32_t height;
};
DrawnSurface sDrawn[2] = {};
int sDrawnCount = 0;

uint8_t sToLinear[256];

void BuildLinearTable()
{
    for (int i = 0; i < 256; ++i) {
        const float value = float(i) / 255.0f;
        const float linear = value <= 0.04045f
                                 ? value / 12.92f
                                 : powf((value + 0.055f) / 1.055f, 2.4f);
        sToLinear[i] = uint8_t(linear * 255.0f + 0.5f);
    }
}

void LinearizeDrawData(ImDrawData* drawData)
{
    for (int list = 0; list < drawData->CmdListsCount; ++list) {
        ImDrawVert* vertices = drawData->CmdLists[list]->VtxBuffer.Data;
        const int count = drawData->CmdLists[list]->VtxBuffer.Size;
        for (int i = 0; i < count; ++i) {
            const ImU32 color = vertices[i].col;
            vertices[i].col =
                (color & IM_COL32_A_MASK) |
                (ImU32(sToLinear[(color >> IM_COL32_R_SHIFT) & 0xFFu])
                 << IM_COL32_R_SHIFT) |
                (ImU32(sToLinear[(color >> IM_COL32_G_SHIFT) & 0xFFu])
                 << IM_COL32_G_SHIFT) |
                (ImU32(sToLinear[(color >> IM_COL32_B_SHIFT) & 0xFFu])
                 << IM_COL32_B_SHIFT);
        }
    }
}

bool EnsureContext()
{
    if (sContextReady)
        return true;
    if (!sOverlayContext) {
        sOverlayContext = static_cast<GX2ContextState*>(
            memalign(GX2_CONTEXT_STATE_ALIGNMENT, sizeof(GX2ContextState)));
        if (!sOverlayContext)
            return false;
    }

    GX2SetupContextStateEx(sOverlayContext, GX2_TRUE);
    DCInvalidateRange(sOverlayContext, sizeof(GX2ContextState));
    sContextReady = true;
    return true;
}

bool AlreadyDrawn(const GX2ColorBuffer* buffer)
{
    for (int i = 0; i < sDrawnCount; ++i) {
        if (sDrawn[i].image == buffer->surface.image &&
            sDrawn[i].width == buffer->surface.width &&
            sDrawn[i].height == buffer->surface.height)
            return true;
    }
    return false;
}

void RememberDrawn(const GX2ColorBuffer* buffer)
{
    if (sDrawnCount < int(sizeof(sDrawn) / sizeof(sDrawn[0]))) {
        sDrawn[sDrawnCount++] = {buffer->surface.image, buffer->surface.width,
                                 buffer->surface.height};
    }
}

FrameSnapshot ReadSnapshot()
{
    FrameSnapshot copy = {};
    for (int attempt = 0; attempt < 4; ++attempt) {
        const uint32_t before =
            sSnapshotSequence.load(std::memory_order_acquire);
        if (before & 1u)
            continue;
        OSMemoryBarrier();
        copy = sSnapshot;
        OSMemoryBarrier();
        if (before == sSnapshotSequence.load(std::memory_order_acquire))
            return copy;
    }
    return copy;
}

void FeedPad(const RplPad& pad)
{
    static const struct {
        uint32_t button;
        ImGuiKey key;
    } mappings[] = {
        {VPAD_BUTTON_UP, ImGuiKey_GamepadDpadUp},
        {VPAD_BUTTON_DOWN, ImGuiKey_GamepadDpadDown},
        {VPAD_BUTTON_LEFT, ImGuiKey_GamepadDpadLeft},
        {VPAD_BUTTON_RIGHT, ImGuiKey_GamepadDpadRight},
        {VPAD_BUTTON_A, ImGuiKey_GamepadFaceDown},
        {VPAD_BUTTON_B, ImGuiKey_GamepadFaceRight},
        {VPAD_BUTTON_L, ImGuiKey_GamepadL1},
        {VPAD_BUTTON_R, ImGuiKey_GamepadR1},
    };

    ImGuiIO& io = ImGui::GetIO();
    for (const auto& mapping : mappings)
        io.AddKeyEvent(mapping.key, (pad.hold & mapping.button) != 0);
}

void BuildWatermark(const FrameSnapshot& frame,
                    const Settings::Snapshot& settings)
{
    if (settings.watermarkOnlyCombo && !frame.comboHeld)
        return;

    ImGui::SetNextWindowPos(ImVec2(kLogicalWidth - 24.0f, 24.0f),
                            ImGuiCond_Always, ImVec2(1.0f, 0.0f));
    ImGui::SetNextWindowBgAlpha(frame.macroActive ? 0.35f : 0.2f);
    ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, ImVec2(10.0f, 5.0f));
    ImGui::PushStyleVar(ImGuiStyleVar_WindowRounding, 4.0f);
    ImGui::PushStyleVar(ImGuiStyleVar_WindowBorderSize, 0.0f);
    ImGui::PushStyleColor(ImGuiCol_Text, ImVec4(0.85f, 0.87f, 0.9f, 0.9f));

    constexpr ImGuiWindowFlags flags =
        ImGuiWindowFlags_NoDecoration | ImGuiWindowFlags_AlwaysAutoResize |
        ImGuiWindowFlags_NoSavedSettings | ImGuiWindowFlags_NoFocusOnAppearing |
        ImGuiWindowFlags_NoNav | ImGuiWindowFlags_NoInputs;

    if (ImGui::Begin("##mss-watermark", nullptr, flags)) {
        ImGui::SetWindowFontScale(kWatermarkScale);
        const char* state = !settings.macroEnabled ? "OFF"
                            : frame.macroActive    ? "ACTIVE"
                                                   : "READY";
        const ImVec4 stateColor = !settings.macroEnabled
                                      ? ImVec4(0.95f, 0.45f, 0.4f, 0.9f)
                                  : frame.macroActive
                                      ? ImVec4(0.25f, 1.0f, 0.38f, 0.9f)
                                      : ImVec4(0.72f, 0.76f, 0.82f, 0.8f);
        ImGui::TextColored(stateColor, "MSS %s", state);
        ImGui::SameLine();
        ImGui::Text("| +%u ms | STICK %s", unsigned(settings.plusDelayMs),
                    settings.stickEnabled ? "ON" : "OFF");
    }
    ImGui::End();
    ImGui::PopStyleColor();
    ImGui::PopStyleVar(3);
}

void BuildSettingsMenu()
{
    if (!sMenuVisible.load(std::memory_order_relaxed))
        return;

    Settings::Snapshot settings = Settings::Get();
    ImGui::SetNextWindowPos(ImVec2(120.0f, 120.0f), ImGuiCond_FirstUseEver);
    ImGui::SetNextWindowSize(ImVec2(650.0f, 0.0f), ImGuiCond_FirstUseEver);
    if (ImGui::Begin("WWHD MSS", nullptr, ImGuiWindowFlags_NoSavedSettings)) {
        bool enabled = settings.macroEnabled;
        if (ImGui::Checkbox("Macro enabled", &enabled))
            Settings::SetMacroEnabled(enabled);

        int delay = int(settings.plusDelayMs);
        if (ImGui::SliderInt("PLUS delay", &delay, 0, 1000, "%d ms",
                             ImGuiSliderFlags_AlwaysClamp))
            Settings::PreviewPlusDelayMs(uint32_t(delay));
        if (ImGui::IsItemDeactivatedAfterEdit())
            Settings::SetPlusDelayMs(Settings::Get().plusDelayMs);

        bool stick = settings.stickEnabled;
        if (ImGui::Checkbox("Automatic stick control", &stick))
            Settings::SetStickEnabled(stick);

        bool park = settings.pauseOnRelease;
        if (ImGui::Checkbox("Pause when the combo is released", &park))
            Settings::SetPauseOnRelease(park);

        bool onlyCombo = settings.watermarkOnlyCombo;
        if (ImGui::Checkbox("Watermark only while the combo is held", &onlyCombo))
            Settings::SetWatermarkOnlyCombo(onlyCombo);

        ImGui::Separator();
        ImGui::Text("Macro settings CRC32: %08X", unsigned(Settings::Crc32()));
        ImGui::TextUnformatted("Hold ZR + A while swimming to run the macro.");
        ImGui::TextUnformatted("ZL + L + Minus closes this menu.");
    }
    ImGui::End();
}

void BuildFrame()
{
    const FrameSnapshot frame = ReadSnapshot();
    const Settings::Snapshot settings = Settings::Get();

    ImGuiIO& io = ImGui::GetIO();
    io.DisplaySize = ImVec2(kLogicalWidth, kLogicalHeight);
    io.DisplayFramebufferScale = ImVec2(1.0f, 1.0f);
    io.DeltaTime = 1.0f / 60.0f;
    FeedPad(frame.pad);

    ImGui_ImplGX2_NewFrame();
    ImGui::NewFrame();
    BuildWatermark(frame, settings);
    BuildSettingsMenu();
    ImGui::Render();
    if (ImDrawData* drawData = ImGui::GetDrawData())
        LinearizeDrawData(drawData);
}

void DrawInto(const GX2ColorBuffer* target)
{
    ImDrawData* drawData = ImGui::GetDrawData();
    if (!drawData || drawData->DisplaySize.x <= 0.0f ||
        drawData->DisplaySize.y <= 0.0f)
        return;

    drawData->FramebufferScale =
        ImVec2(float(target->surface.width) / drawData->DisplaySize.x,
               float(target->surface.height) / drawData->DisplaySize.y);

    GX2ColorBuffer bound = *target;
    GX2InitColorBufferRegs(&bound);
    GX2SetColorBuffer(&bound, GX2_RENDER_TARGET_0);
    GX2SetViewport(0.0f, 0.0f, float(target->surface.width),
                   float(target->surface.height), 0.0f, 1.0f);
    GX2SetScissor(0, 0, target->surface.width, target->surface.height);
    GX2SetShaderMode(GX2_SHADER_MODE_UNIFORM_REGISTER);
    GX2SetDepthOnlyControl(GX2_FALSE, GX2_FALSE, GX2_COMPARE_FUNC_NEVER);
    GX2SetAlphaTest(GX2_TRUE, GX2_COMPARE_FUNC_GREATER, 0.0f);
    GX2SetColorControl(GX2_LOGIC_OP_COPY, GX2_ENABLE, GX2_DISABLE, GX2_ENABLE);

    ImGui_ImplGX2_RenderDrawData(drawData);
}

} // namespace

void Init()
{
    if (!sImGuiReady) {
        IMGUI_CHECKVERSION();
        ImGui::CreateContext();

        ImGuiIO& io = ImGui::GetIO();
        io.ConfigFlags |= ImGuiConfigFlags_NavEnableGamepad;
        io.BackendFlags |= ImGuiBackendFlags_HasGamepad;
        io.IniFilename = nullptr;
        io.LogFilename = nullptr;
        io.DisplaySize = ImVec2(kLogicalWidth, kLogicalHeight);

        ImGui::StyleColorsDark();
        ImGuiStyle& style = ImGui::GetStyle();
        style.ScaleAllSizes(2.0f);
        io.FontGlobalScale = 2.0f;

        BuildLinearTable();
        ImGui_ImplGX2_Init();
        sImGuiReady = true;
    }

    OSMemoryBarrier();
    sReady.store(true, std::memory_order_release);
}

void Shutdown()
{
    sReady.store(false, std::memory_order_release);
    OSMemoryBarrier();
    if (sHost && sHost->setInputMode)
        sHost->setInputMode(sHost, RPL_INPUT_PASS);
    sHost = nullptr;
}

void OnReleaseForeground()
{
    sReady.store(false, std::memory_order_release);
    OSMemoryBarrier();
    if (sHost && sHost->setInputMode)
        sHost->setInputMode(sHost, RPL_INPUT_PASS);

    if (sImGuiReady)
        ImGui_ImplGX2_DestroyDeviceObjects();
    sGameContext = nullptr;
    sContextReady = false;
    sBuiltFrameSerial = UINT32_MAX;
}

void OnAcquiredForeground()
{
    sGameContext = nullptr;
    sContextReady = false;
    sBuiltFrameSerial = UINT32_MAX;
    if (sHost && sHost->setInputMode) {
        sHost->setInputMode(
            sHost, sMenuVisible.load(std::memory_order_relaxed)
                       ? RPL_INPUT_BLOCK
                       : RPL_INPUT_PASS);
    }
    OSMemoryBarrier();
    sReady.store(true, std::memory_order_release);
}

void Tick(bool comboHeld, bool macroActive, const MacroOutput& output)
{
    if (Input::Pressed(kMenuToggle)) {
        const bool visible = !sMenuVisible.load(std::memory_order_relaxed);
        sMenuVisible.store(visible, std::memory_order_relaxed);
        if (sHost && sHost->setInputMode) {
            sHost->setInputMode(sHost, visible ? RPL_INPUT_BLOCK
                                               : RPL_INPUT_PASS);
        }
    }

    sSnapshotSequence.fetch_add(1, std::memory_order_acq_rel);
    OSMemoryBarrier();
    sSnapshot.pad = Input::Pad();
    sSnapshot.comboHeld = comboHeld;
    sSnapshot.macroActive = macroActive;
    sSnapshot.plusHeld = output.plusHeld;
    sSnapshot.stickDown = output.stickY < 0.0f;
    OSMemoryBarrier();
    sSnapshotSequence.fetch_add(1, std::memory_order_release);
    sFrameSerial.fetch_add(1, std::memory_order_release);
}

bool IsMenuVisible()
{
    return sMenuVisible.load(std::memory_order_relaxed);
}

void NoteGameContext(GX2ContextState* state)
{
    if (state && state != sOverlayContext)
        sGameContext = state;
}

void OnPresent(CopyFn copy, const GX2ColorBuffer* buffer, GX2ScanTarget target)
{
    if (!copy)
        return;
    if (!sReady.load(std::memory_order_acquire) || !buffer ||
        (target != GX2_SCAN_TARGET_TV && target != GX2_SCAN_TARGET_DRC)) {
        copy(buffer, target);
        return;
    }

    GX2ContextState* const restore = sGameContext;
    if (!restore || !EnsureContext()) {
        copy(buffer, target);
        return;
    }

    if (!buffer->surface.image || !buffer->surface.width ||
        !buffer->surface.height) {
        copy(buffer, target);
        return;
    }

    GX2SetContextState(sOverlayContext);
    GX2SetDefaultState();

    const uint32_t frameSerial = sFrameSerial.load(std::memory_order_acquire);
    if (sBuiltFrameSerial != frameSerial) {
        sDrawnCount = 0;
        BuildFrame();
        sBuiltFrameSerial = frameSerial;
    }
    if (!AlreadyDrawn(buffer)) {
        DrawInto(buffer);
        RememberDrawn(buffer);
    }

    GX2Flush();
    GX2SetContextState(restore);
    copy(buffer, target);
}

void BindHost(const RplHost* host)
{
    sHost = host;
}

} // namespace Overlay
} // namespace Mss

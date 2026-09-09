#include <coreinit/debug.h>
#include <coreinit/dynload.h>
#include <coreinit/time.h>
#include <gx2/context.h>
#include <gx2/surface.h>
#include <gx2/swap.h>
#include <vpad/input.h>

#include "libwwhd/libwwhd.h"
#include <rplloader/rplloader.h>

#include "input.h"
#include "macro_engine.h"
#include "overlay.h"
#include "settings.h"

namespace Mss {
namespace {

const RplHost* sHost;
bool sStickSet;
bool sButtonsSet;
MacroEngine sMacro;

struct RplTitleBlob {
    uint32_t magic;
    uint32_t version;
    uint32_t count;
    uint32_t reserved;
    uint64_t ids[3];
};

const RplTitleBlob kTitleBlob
    __attribute__((section(RPL_TITLES_SECTION), used, aligned(8))) = {
    RPL_TITLES_MAGIC, 1u, 3u, 0u,
    {
        0x0005000010143500ull,
        0x0005000010143600ull,
        0x0005000010143400ull,
    },
};

void ClearStick()
{
    if (!sHost || !sStickSet)
        return;
    sHost->setStick(sHost, nullptr);
    sStickSet = false;
}

void ClearButtons()
{
    if (!sHost || !sButtonsSet || !sHost->setButtons)
        return;
    sHost->setButtons(sHost, 0);
    sButtonsSet = false;
}

void ClearMacroOutput()
{
    ClearStick();
    ClearButtons();
}

bool ReversalSeen()
{
    static int16_t previous;
    const uint8_t* link = reinterpret_cast<const uint8_t*>(daPy_lk_c_getPlayer());
    if (!link)
        return false;
    const int16_t target = *reinterpret_cast<const int16_t*>(link + 0x32a);
    const int16_t turned = int16_t(target - previous);
    previous = target;
    return turned > 0x4000 || turned < -0x4000;
}

void LogSettingsCrc(const Settings::Snapshot& settings)
{
    static uint32_t logged;
    const uint32_t crc = Settings::Crc32();
    if (crc == logged)
        return;
    logged = crc;
    sHost->log(sHost, RPL_LOG_INFO, "macro settings crc32 %08X: plus delay %u ms, stick %s",
               unsigned(crc), unsigned(settings.plusDelayMs),
               settings.stickEnabled ? "on" : "off");
}

void Update()
{
    if (!sHost)
        return;

    Input::Poll();

    const Settings::Snapshot settings = Settings::Get();
    MacroSettings macroSettings;
    macroSettings.enabled = settings.macroEnabled;
    macroSettings.plusDelayMs = settings.plusDelayMs;
    macroSettings.stickEnabled = settings.stickEnabled;
    macroSettings.stickStrength = 1.0f;
    macroSettings.pauseOnRelease = settings.pauseOnRelease;
    sMacro.SetSettings(macroSettings);

    const bool comboHeld = Input::Held(VPAD_BUTTON_A | VPAD_BUTTON_ZR);
    bool active = comboHeld;
    if (Overlay::IsMenuVisible())
        active = false;

    const int32_t proc = daPy_getCurProc();
    if (proc != int32_t(daPyProc_SWIM_WAIT_e) &&
        proc != int32_t(daPyProc_SWIM_MOVE_e))
        active = false;

    const uint64_t nowMs = OSTicksToMilliseconds(OSGetTime());
    const MacroOutput output = sMacro.Step(active, nowMs, ReversalSeen());

    if (output.plusHeld) {
        sHost->setButtons(sHost, VPAD_BUTTON_PLUS);
        sButtonsSet = true;
    } else {
        ClearButtons();
    }

    if (output.stickOverride) {
        const float stick[2] = {output.stickX, output.stickY};
        sHost->setStick(sHost, stick);
        sStickSet = true;
    } else {
        ClearStick();
    }

    Overlay::Tick(comboHeld, active && settings.macroEnabled, output);
    LogSettingsCrc(settings);
}

RPL_DECL_REPLACE(void, cCt_Counter, int reset)
{
    real_cCt_Counter(reset);
    Update();
}

RPL_DECL_REPLACE(void, GX2CopyColorBufferToScanBuffer,
                 const GX2ColorBuffer* buffer, GX2ScanTarget target)
{
    if (real_GX2CopyColorBufferToScanBuffer) {
        Overlay::OnPresent(real_GX2CopyColorBufferToScanBuffer, buffer, target);
    }
}

RPL_DECL_REPLACE(void, GX2SetContextState, GX2ContextState* state)
{
    Overlay::NoteGameContext(state);
    if (real_GX2SetContextState)
        real_GX2SetContextState(state);
}

const RplHook kHooks[] = {
    RPL_REPLACE(cCt_Counter, 0x0200E6ECu, 0x3D401020u, RPL_HOOK_REQUIRED),
    RPL_REPLACE_LIB(GX2CopyColorBufferToScanBuffer, "gx2", RPL_HOOK_REQUIRED),
    RPL_REPLACE_LIB(GX2SetContextState, "gx2", RPL_HOOK_REQUIRED),
};

int OnInit(const RplHost* host)
{
    sHost = host;
    sStickSet = false;
    sButtonsSet = false;
    sMacro.Reset();

    if (!host->setButtons) {
        host->log(host, RPL_LOG_ERROR,
                  "loader does not provide ABI 6 button injection");
        return -1;
    }

    wwhd_textDelta = host->textDelta(host);
    wwhd_textResolved = 1;
    wwhd_dataDelta = host->dataDelta(host);
    wwhd_dataResolved = 1;
    wwhd_titleId = host->titleId(host);

    if (wwhd_selectRegion(uint32_t(wwhd_titleId)) == WWHD_REGION_NONE)
        return -1;

    Input::Bind(host);
    Settings::Init(host);
    Overlay::BindHost(host);
    Overlay::Init();

    host->log(host, RPL_LOG_INFO,
              "region %s: hold ZR+A while swimming; ZL+L+Minus opens settings",
              wwhd_regionName());
    return 0;
}

void OnDeinit()
{
    ClearMacroOutput();
    sMacro.Reset();
    Overlay::Shutdown();
    Input::Bind(nullptr);
    sHost = nullptr;
}

void OnReleaseForeground()
{
    ClearMacroOutput();
    sMacro.Reset();
    Input::Reset();
    Overlay::OnReleaseForeground();
}

void OnAcquiredForeground()
{
    Overlay::OnAcquiredForeground();
}

const RplManifest kManifest = {
    .magic = RPL_MAGIC,
    .abiVersion = RPL_ABI_VERSION,
    .name = "wwhd_mss",
    .version = "0.2",
    .author = "rpl-loader examples by n0ted",
    .titleIds = kTitleBlob.ids,
    .titleIdCount = sizeof(kTitleBlob.ids) / sizeof(kTitleBlob.ids[0]),
    .hooks = kHooks,
    .hookCount = sizeof(kHooks) / sizeof(kHooks[0]),
    .priority = 0,
    .flags = RPL_FLAG_ALLOW_RELEASE,
    .onInit = OnInit,
    .onDeinit = OnDeinit,
    .maxHooks = 3,
    .onReleaseForeground = OnReleaseForeground,
    .onAcquiredForeground = OnAcquiredForeground,
    .onPadSampled = nullptr,
};

}

void OnCemuFrame()
{
    Update();
}

}

RPL_MANIFEST(Mss::kManifest)

RPL_EXPORT int rpl_entry(OSDynLoad_Module module, OSDynLoad_EntryReason reason)
{
    (void)module;
    (void)reason;
    return 0;
}

void ImGuiRplAssert(const char* expression, const char* file, int line)
{
    OSReport("[wwhd_mss] ImGui assert: %s (%s:%d)\n", expression, file, line);
}

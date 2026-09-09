#include <coreinit/debug.h>
#include <coreinit/title.h>
#include <gx2/context.h>
#include <gx2/surface.h>
#include <gx2/swap.h>
#include <padscore/kpad.h>
#include <padscore/wpad.h>
#include <vpad/input.h>

#include <cstdarg>
#include <cstdio>
#include <cstdlib>
#include <cstring>

#include <rplloader/rplloader.h>

#include "input.h"
#include "macro_engine.h"
#include "overlay.h"
#include "settings.h"

RPL_EXPORT const RplManifest* rpl_manifest();

namespace Mss {

void OnCemuFrame();
}

namespace Cemu {
namespace {

enum {
    RPL_CEMU_FRAME = 0,
    RPL_CEMU_VPAD = 2,
    RPL_CEMU_KPAD = 4,
    RPL_CEMU_GX2_COPY = 6,
    RPL_CEMU_GX2_CONTEXT = 8,
};

RplHost sHost;
RplPad sPad;
RplKpad sKpad[RPL_KPAD_CHANNELS];
bool sHavePad;
bool sHaveKpad[RPL_KPAD_CHANNELS];
bool sStickHeld;
float sStickX;
float sStickY;
int sInputMode;
uint32_t sButtonsHeld;
uint32_t sVpadButtonsApplied;
uint32_t sKpadButtonsApplied[RPL_KPAD_CHANNELS];
bool sReady;
bool sFailed;
bool sInitialising;

struct CemuConfig {
    bool macroEnabled = true;
    int32_t plusDelayMs = int32_t(Mss::kDefaultPlusDelayMs);
    bool stickEnabled = true;
    bool pauseOnRelease = true;
    bool watermarkOnlyCombo = true;
};

CemuConfig sConfig;

const char* ValueFor(const char* line, const char* key)
{
    const size_t length = std::strlen(key);
    return std::strncmp(line, key, length) == 0 ? line + length : nullptr;
}

bool ParseBool(const char* value, bool fallback)
{
    if (!value)
        return fallback;
    if (std::strncmp(value, "1", 1) == 0 ||
        std::strncmp(value, "true", 4) == 0 ||
        std::strncmp(value, "on", 2) == 0)
        return true;
    if (std::strncmp(value, "0", 1) == 0 ||
        std::strncmp(value, "false", 5) == 0 ||
        std::strncmp(value, "off", 3) == 0)
        return false;
    return fallback;
}

void LoadConfig()
{
    std::FILE* file = std::fopen("wwhd_mss.cfg", "r");
    if (!file)
        return;

    char line[96];
    while (std::fgets(line, sizeof(line), file)) {
        if (const char* value = ValueFor(line, "macro_enabled=")) {
            sConfig.macroEnabled = ParseBool(value, sConfig.macroEnabled);
        } else if (const char* value = ValueFor(line, "plus_delay_ms=")) {
            long delay = std::strtol(value, nullptr, 10);
            if (delay < 0)
                delay = 0;
            if (delay > long(Mss::kMaximumPlusDelayMs))
                delay = long(Mss::kMaximumPlusDelayMs);
            sConfig.plusDelayMs = int32_t(delay);
        } else if (const char* value = ValueFor(line, "stick_enabled=")) {
            sConfig.stickEnabled = ParseBool(value, sConfig.stickEnabled);
        } else if (const char* value = ValueFor(line, "pause_on_release=")) {
            sConfig.pauseOnRelease = ParseBool(value, sConfig.pauseOnRelease);
        } else if (const char* value = ValueFor(line, "watermark_only_combo=")) {
            sConfig.watermarkOnlyCombo = ParseBool(value, sConfig.watermarkOnlyCombo);
        }
    }
    std::fclose(file);
}

bool SaveConfig()
{
    std::FILE* file = std::fopen("wwhd_mss.cfg", "w");
    if (!file)
        return false;
    std::fprintf(file, "# WWHD MSS settings (Cemu)\n");
    std::fprintf(file, "macro_enabled=%d\n", sConfig.macroEnabled ? 1 : 0);
    std::fprintf(file, "plus_delay_ms=%d\n", int(sConfig.plusDelayMs));
    std::fprintf(file, "stick_enabled=%d\n", sConfig.stickEnabled ? 1 : 0);
    std::fprintf(file, "pause_on_release=%d\n", sConfig.pauseOnRelease ? 1 : 0);
    std::fprintf(file, "watermark_only_combo=%d\n",
                 sConfig.watermarkOnlyCombo ? 1 : 0);
    const bool ok = std::fclose(file) == 0;
    return ok;
}

void HostLog(const RplHost*, int level, const char* format, ...)
{
    char message[512];
    va_list args;
    va_start(args, format);
    std::vsnprintf(message, sizeof(message), format, args);
    va_end(args);

    const char* kind = level == RPL_LOG_ERROR ? "error" :
                       level == RPL_LOG_WARN ? "warn" : "info";
    OSReport("[wwhd_mss][%s] %s\n", kind, message);
}

uint64_t HostTitleId(const RplHost*)
{
    return OSGetTitleID();
}

uint32_t HostDelta(const RplHost*)
{
    return 0;
}

int HostPad(const RplHost*, RplPad* out)
{
    if (!out || !sHavePad)
        return 0;
    *out = sPad;
    return 1;
}

int HostKpad(const RplHost*, uint32_t chan, RplKpad* out)
{
    if (!out || chan >= RPL_KPAD_CHANNELS || !sHaveKpad[chan])
        return 0;
    *out = sKpad[chan];
    return 1;
}

void HostSetStick(const RplHost*, const float* stick)
{
    if (!stick) {
        sStickHeld = false;
        return;
    }
    sStickX = stick[0];
    sStickY = stick[1];
    sStickHeld = true;
}

void HostSetInputMode(const RplHost*, int mode)
{
    sInputMode = mode == RPL_INPUT_BLOCK ? RPL_INPUT_BLOCK : RPL_INPUT_PASS;
}

void HostSetButtons(const RplHost*, uint32_t buttons)
{
    sButtonsHeld = buttons;
}

int HostGetBool(const RplHost*, const char* key, int fallback)
{
    if (!key)
        return fallback;
    if (std::strcmp(key, Mss::Settings::kMacroEnabledKey) == 0)
        return sConfig.macroEnabled ? 1 : 0;
    if (std::strcmp(key, Mss::Settings::kStickEnabledKey) == 0)
        return sConfig.stickEnabled ? 1 : 0;
    if (std::strcmp(key, Mss::Settings::kPauseOnReleaseKey) == 0)
        return sConfig.pauseOnRelease ? 1 : 0;
    if (std::strcmp(key, Mss::Settings::kWatermarkOnlyComboKey) == 0)
        return sConfig.watermarkOnlyCombo ? 1 : 0;
    return fallback;
}

int HostSetBool(const RplHost*, const char* key, int value)
{
    if (!key)
        return -1;
    if (std::strcmp(key, Mss::Settings::kMacroEnabledKey) == 0)
        sConfig.macroEnabled = value != 0;
    else if (std::strcmp(key, Mss::Settings::kStickEnabledKey) == 0)
        sConfig.stickEnabled = value != 0;
    else if (std::strcmp(key, Mss::Settings::kPauseOnReleaseKey) == 0)
        sConfig.pauseOnRelease = value != 0;
    else if (std::strcmp(key, Mss::Settings::kWatermarkOnlyComboKey) == 0)
        sConfig.watermarkOnlyCombo = value != 0;
    else
        return -1;
    return SaveConfig() ? 0 : -1;
}

int32_t HostGetInt(const RplHost*, const char* key, int32_t fallback)
{
    if (key && std::strcmp(key, Mss::Settings::kPlusDelayKey) == 0)
        return sConfig.plusDelayMs;
    return fallback;
}

int HostSetInt(const RplHost*, const char* key, int32_t value)
{
    if (!key || std::strcmp(key, Mss::Settings::kPlusDelayKey) != 0)
        return -1;
    if (value < 0)
        value = 0;
    if (uint32_t(value) > Mss::kMaximumPlusDelayMs)
        value = int32_t(Mss::kMaximumPlusDelayMs);
    sConfig.plusDelayMs = value;
    return SaveConfig() ? 0 : -1;
}

constexpr uint32_t kClassicButtons =
    ~uint32_t(WPAD_PRO_BUTTON_STICK_L | WPAD_PRO_BUTTON_STICK_R);

void EditButtons(uint32_t& hold, uint32_t& trigger, uint32_t& release,
                 uint32_t current, uint32_t previous)
{
    const uint32_t physicalHold = hold;
    const uint32_t physicalRelease = release;
    trigger &= ~previous;
    release &= ~current;
    trigger |= (current & ~previous) & ~(physicalHold | physicalRelease);
    release |= (previous & ~current) & ~physicalHold;
    hold = physicalHold | current;
}

void BuildHost()
{
    sConfig = {};
    sPad = {};
    std::memset(sKpad, 0, sizeof(sKpad));
    sHavePad = false;
    std::memset(sHaveKpad, 0, sizeof(sHaveKpad));
    sStickHeld = false;
    sInputMode = RPL_INPUT_PASS;
    sButtonsHeld = 0;
    sVpadButtonsApplied = 0;
    std::memset(sKpadButtonsApplied, 0, sizeof(sKpadButtonsApplied));
    LoadConfig();

    sHost = {};
    sHost.version = RPL_ABI_VERSION;
    sHost.name = "wwhd_mss";
    sHost.dir = "";
    sHost.log = HostLog;
    sHost.titleId = HostTitleId;
    sHost.textDelta = HostDelta;
    sHost.dataDelta = HostDelta;
    sHost.getBool = HostGetBool;
    sHost.setBool = HostSetBool;
    sHost.pad = HostPad;
    sHost.kpad = HostKpad;
    sHost.setInputMode = HostSetInputMode;
    sHost.setStick = HostSetStick;
    sHost.setButtons = HostSetButtons;
    sHost.getInt = HostGetInt;
    sHost.setInt = HostSetInt;
}

bool EnsureReady()
{
    if (sReady)
        return true;
    if (sFailed || sInitialising)
        return false;

    sInitialising = true;
    const RplManifest* manifest = rpl_manifest();
    if (!manifest || !manifest->onInit) {
        sFailed = true;
        sInitialising = false;
        return false;
    }

    BuildHost();
    const int result = manifest->onInit(&sHost);
    sInitialising = false;
    if (result != 0) {
        OSReport("[wwhd_mss] onInit returned %d under Cemu\n", result);
        sFailed = true;
        return false;
    }

    sReady = true;
    return true;
}

void NoteVpad(VPADStatus* buffers, uint32_t count)
{
    if (!buffers || count == 0)
        return;

    sPad.hold = buffers[0].hold;
    sPad.trigger = buffers[0].trigger;
    sPad.release = buffers[0].release;
    sPad.leftX = buffers[0].leftStick.x;
    sPad.leftY = buffers[0].leftStick.y;
    sPad.rightX = buffers[0].rightStick.x;
    sPad.rightY = buffers[0].rightStick.y;
    sPad.touch.x = buffers[0].tpNormal.x;
    sPad.touch.y = buffers[0].tpNormal.y;
    sPad.touch.touched = buffers[0].tpNormal.touched;
    sPad.touch.validity = buffers[0].tpNormal.validity;
    ++sPad.sample;
    sHavePad = true;

    const bool block = sInputMode == RPL_INPUT_BLOCK;
    const uint32_t previous = sVpadButtonsApplied;
    sVpadButtonsApplied = sButtonsHeld;

    for (uint32_t i = 0; i < count; ++i) {
        VPADStatus& status = buffers[i];
        if (block) {
            status.hold = status.trigger = status.release = 0;
            status.rightStick.x = status.rightStick.y = 0.0f;
            status.tpNormal.touched = 0;
            status.tpFiltered1.touched = 0;
            status.tpFiltered2.touched = 0;
        }
        if (sStickHeld) {
            status.leftStick.x = sStickX;
            status.leftStick.y = sStickY;
        } else if (block) {
            status.leftStick.x = status.leftStick.y = 0.0f;
        }
        EditButtons(status.hold, status.trigger, status.release, sButtonsHeld,
                    i == 0 ? previous : sButtonsHeld);
    }
}

void NoteKpad(KPADStatus* buffers, uint32_t count, uint32_t chan)
{
    if (!buffers || count == 0 || chan >= RPL_KPAD_CHANNELS)
        return;

    RplKpad& out = sKpad[chan];
    out.extension = uint32_t(buffers[0].extensionType);
    out.hold = 0;
    out.trigger = 0;
    out.release = 0;
    if (buffers[0].extensionType == WPAD_EXT_PRO_CONTROLLER) {
        out.hold = buffers[0].pro.hold;
        out.trigger = buffers[0].pro.trigger;
        out.release = buffers[0].pro.release;
        out.leftX = buffers[0].pro.leftStick.x;
        out.leftY = buffers[0].pro.leftStick.y;
        out.rightX = buffers[0].pro.rightStick.x;
        out.rightY = buffers[0].pro.rightStick.y;
    } else if (buffers[0].extensionType == WPAD_EXT_CLASSIC ||
               buffers[0].extensionType == WPAD_EXT_MPLUS_CLASSIC) {
        out.hold = buffers[0].classic.hold;
        out.trigger = buffers[0].classic.trigger;
        out.release = buffers[0].classic.release;
        out.leftX = buffers[0].classic.leftStick.x;
        out.leftY = buffers[0].classic.leftStick.y;
        out.rightX = buffers[0].classic.rightStick.x;
        out.rightY = buffers[0].classic.rightStick.y;
    }
    ++out.sample;
    sHaveKpad[chan] = true;

    const bool block = sInputMode == RPL_INPUT_BLOCK;
    const uint32_t previous = sKpadButtonsApplied[chan];
    sKpadButtonsApplied[chan] = sButtonsHeld;

    for (uint32_t i = 0; i < count; ++i) {
        KPADStatus& status = buffers[i];
        if (block) {
            status.hold = status.trigger = status.release = 0;
            status.pro.hold = status.pro.trigger = status.pro.release = 0;
            status.classic.hold = status.classic.trigger = status.classic.release = 0;
            status.nunchuk.hold = status.nunchuk.trigger = status.nunchuk.release = 0;
            status.pro.rightStick.x = status.pro.rightStick.y = 0.0f;
            status.classic.rightStick.x = status.classic.rightStick.y = 0.0f;
        }
        if (sStickHeld) {
            status.pro.leftStick.x = sStickX;
            status.pro.leftStick.y = sStickY;
            status.classic.leftStick.x = sStickX;
            status.classic.leftStick.y = sStickY;
        } else if (block) {
            status.pro.leftStick.x = status.pro.leftStick.y = 0.0f;
            status.classic.leftStick.x = status.classic.leftStick.y = 0.0f;
            status.nunchuk.stick.x = status.nunchuk.stick.y = 0.0f;
        }

        const uint32_t prior = i == 0 ? previous : sButtonsHeld;
        const uint32_t current = Mss::Input::WpadFromVpad(sButtonsHeld);
        const uint32_t before = Mss::Input::WpadFromVpad(prior);
        if (status.extensionType == WPAD_EXT_PRO_CONTROLLER) {
            EditButtons(status.pro.hold, status.pro.trigger, status.pro.release,
                        current, before);
        } else if (status.extensionType == WPAD_EXT_CLASSIC ||
                   status.extensionType == WPAD_EXT_MPLUS_CLASSIC) {
            EditButtons(status.classic.hold, status.classic.trigger,
                        status.classic.release, current & kClassicButtons,
                        before & kClassicButtons);
        }
    }
}

}
}

RPL_EXPORT uint32_t rpl_cemu_entry(uint32_t reason, void* a, void* b, void* c)
{
    if (!Cemu::EnsureReady())
        return 0;

    switch (reason) {
    case Cemu::RPL_CEMU_FRAME:
        Mss::OnCemuFrame();
        return 1;
    case Cemu::RPL_CEMU_VPAD:
        Cemu::NoteVpad(static_cast<VPADStatus*>(a),
                       uint32_t(reinterpret_cast<uintptr_t>(b)));
        return 1;
    case Cemu::RPL_CEMU_KPAD:
        Cemu::NoteKpad(static_cast<KPADStatus*>(a),
                       uint32_t(reinterpret_cast<uintptr_t>(b)),
                       uint32_t(reinterpret_cast<uintptr_t>(c)));
        return 1;
    case Cemu::RPL_CEMU_GX2_COPY:
        Mss::Overlay::OnPresent(
            GX2CopyColorBufferToScanBuffer,
            static_cast<const GX2ColorBuffer*>(a),
            GX2ScanTarget(uint32_t(reinterpret_cast<uintptr_t>(b))));
        return 1;
    case Cemu::RPL_CEMU_GX2_CONTEXT:
        Mss::Overlay::NoteGameContext(static_cast<GX2ContextState*>(a));
        GX2SetContextState(static_cast<GX2ContextState*>(a));
        return 1;
    default:
        return 0;
    }
}

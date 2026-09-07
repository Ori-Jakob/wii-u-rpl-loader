#include <coreinit/debug.h>
#include <coreinit/title.h>
#include <padscore/kpad.h>
#include <padscore/wpad.h>
#include <vpad/input.h>

#include <cstdarg>
#include <cstdio>

#include <rplloader/rplloader.h>

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
};

RplHost sHost;
RplPad sPad;
RplKpad sKpad[RPL_KPAD_CHANNELS];
bool sHavePad;
bool sHaveKpad[RPL_KPAD_CHANNELS];
bool sStickHeld;
float sStickX;
float sStickY;
bool sReady;
bool sFailed;
bool sInitialising;

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

void BuildHost()
{
    sHost = {};
    sHost.version = RPL_ABI_VERSION;
    sHost.name = "wwhd_mss";
    sHost.dir = "";
    sHost.log = HostLog;
    sHost.titleId = HostTitleId;
    sHost.textDelta = HostDelta;
    sHost.dataDelta = HostDelta;
    sHost.pad = HostPad;
    sHost.kpad = HostKpad;
    sHost.setStick = HostSetStick;
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
    ++sPad.sample;
    sHavePad = true;

    if (!sStickHeld)
        return;

    for (uint32_t i = 0; i < count; ++i) {
        buffers[i].leftStick.x = sStickX;
        buffers[i].leftStick.y = sStickY;
    }
}

void NoteKpad(KPADStatus* buffers, uint32_t count, uint32_t chan)
{
    if (!buffers || count == 0 || chan >= RPL_KPAD_CHANNELS)
        return;

    RplKpad& out = sKpad[chan];
    out.extension = uint32_t(buffers[0].extensionType);
    out.hold = 0;
    if (buffers[0].extensionType == WPAD_EXT_PRO_CONTROLLER)
        out.hold = buffers[0].pro.hold;
    else if (buffers[0].extensionType == WPAD_EXT_CLASSIC ||
             buffers[0].extensionType == WPAD_EXT_MPLUS_CLASSIC)
        out.hold = buffers[0].classic.hold;
    ++out.sample;
    sHaveKpad[chan] = true;

    if (!sStickHeld)
        return;

    for (uint32_t i = 0; i < count; ++i) {
        buffers[i].pro.leftStick.x = sStickX;
        buffers[i].pro.leftStick.y = sStickY;
        buffers[i].classic.leftStick.x = sStickX;
        buffers[i].classic.leftStick.y = sStickY;
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
    default:
        return 0;
    }
}

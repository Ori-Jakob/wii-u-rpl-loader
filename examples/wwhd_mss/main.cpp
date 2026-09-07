#include <coreinit/debug.h>
#include <coreinit/dynload.h>
#include <padscore/wpad.h>
#include <vpad/input.h>

#include "libwwhd/libwwhd.h"
#include <rplloader/rplloader.h>

namespace Mss {
namespace {

const RplHost* sHost;
bool sUp = true;
bool sStickSet;
constexpr uint32_t kButtonA = 1u << 0;
constexpr uint32_t kButtonZr = 1u << 1;

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

uint32_t ReadHeld()
{
    uint32_t held = 0;

    RplPad pad;
    if (sHost->pad(sHost, &pad)) {
        if (pad.hold & VPAD_BUTTON_A)
            held |= kButtonA;
        if (pad.hold & VPAD_BUTTON_ZR)
            held |= kButtonZr;
    }

    for (uint32_t chan = 0; chan < RPL_KPAD_CHANNELS; ++chan) {
        RplKpad pad;
        if (!sHost->kpad(sHost, chan, &pad))
            continue;
        if (pad.extension == WPAD_EXT_PRO_CONTROLLER) {
            if (pad.hold & WPAD_PRO_BUTTON_A)
                held |= kButtonA;
            if (pad.hold & WPAD_PRO_TRIGGER_ZR)
                held |= kButtonZr;
        } else if (pad.extension == WPAD_EXT_CLASSIC ||
                   pad.extension == WPAD_EXT_MPLUS_CLASSIC) {
            if (pad.hold & WPAD_CLASSIC_BUTTON_A)
                held |= kButtonA;
            if (pad.hold & WPAD_CLASSIC_BUTTON_ZR)
                held |= kButtonZr;
        }
    }

    return held;
}

void Update()
{
    if (!sHost) {
        ClearStick();
        return;
    }

    constexpr uint32_t combo = kButtonA | kButtonZr;
    if ((ReadHeld() & combo) != combo) {
        ClearStick();
        return;
    }

    const int32_t proc = daPy_getCurProc();
    if (proc != int32_t(daPyProc_SWIM_WAIT_e) &&
        proc != int32_t(daPyProc_SWIM_MOVE_e)) {
        ClearStick();
        return;
    }

    const float stick[2] = { 0.0f, sUp ? 1.0f : -1.0f };
    sHost->setStick(sHost, stick);
    sStickSet = true;
    sUp = !sUp;
}

RPL_DECL_REPLACE(void, cCt_Counter, int reset)
{
    real_cCt_Counter(reset);
    Update();
}

const RplHook kHooks[] = {
    RPL_REPLACE(cCt_Counter, 0x0200E6ECu, 0x3D401020u, RPL_HOOK_REQUIRED),
};

int OnInit(const RplHost* host)
{
    sHost = host;
    sUp = true;
    sStickSet = false;

    wwhd_textDelta = host->textDelta(host);
    wwhd_textResolved = 1;
    wwhd_dataDelta = host->dataDelta(host);
    wwhd_dataResolved = 1;
    wwhd_titleId = host->titleId(host);

    if (wwhd_selectRegion(uint32_t(wwhd_titleId)) == WWHD_REGION_NONE)
        return -1;

    host->log(host, RPL_LOG_INFO, "region %s: hold ZR+A while swimming for MSS",
              wwhd_regionName());
    return 0;
}

void OnDeinit()
{
    ClearStick();
    sHost = nullptr;
}

const RplManifest kManifest = {
    .magic = RPL_MAGIC,
    .abiVersion = RPL_ABI_VERSION,
    .name = "wwhd_mss",
    .version = "0.1",
    .author = "rpl-loader examples",
    .titleIds = kTitleBlob.ids,
    .titleIdCount = sizeof(kTitleBlob.ids) / sizeof(kTitleBlob.ids[0]),
    .hooks = kHooks,
    .hookCount = sizeof(kHooks) / sizeof(kHooks[0]),
    .priority = 0,
    .flags = RPL_FLAG_ALLOW_RELEASE,
    .onInit = OnInit,
    .onDeinit = OnDeinit,
    .maxHooks = 1,
    .onReleaseForeground = nullptr,
    .onAcquiredForeground = nullptr,
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
    OSReport("[wwhd_mss] rpl_entry(reason %d)\n", int(reason));
    return 0;
}

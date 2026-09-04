#include <coreinit/debug.h>
#include <coreinit/dynload.h>

#include "libwwhd/libwwhd.h"
#include "wwhd_cheats.h"

namespace Cheats {

const RplHost* gHost = nullptr;

namespace {

const uint64_t kTitles[] = {
    0x0005000010143500ull,   // USA
    0x0005000010143600ull,   // EUR
    0x0005000010143400ull,   // JAP
};

int OnInit(const RplHost* host)
{
    gHost = host;

    // The region probe reads .text so both deltas have to be set first
    wwhd_textDelta    = host->textDelta(host);
    wwhd_textResolved = 1;
    wwhd_dataDelta    = host->dataDelta(host);
    wwhd_dataResolved = 1;
    wwhd_titleId      = host->titleId(host);

    if (wwhd_selectRegion(u32(wwhd_titleId & 0xFFFFFFFFull)) == WWHD_REGION_NONE) {
        Log(RPL_LOG_ERROR, "region probe failed at text delta %08X", unsigned(wwhd_textDelta));
        return -1;
    }

    LoadConfig(*host);
    Log(RPL_LOG_INFO, "region %s: L3 for moon jump, ZR aboard for sail x%d",
        wwhd_regionName(), gConfig.sailBoost);
    return 0;
}

void OnDeinit()
{
    ResetHooks();
    gHost = nullptr;
}

RplManifest gManifest = {
    .magic        = RPL_MAGIC,
    .abiVersion   = RPL_ABI_VERSION,
    .name         = "wwhd_cheats",
    .version      = "0.1",
    .author       = "rpl-loader examples",
    .titleIds     = kTitles,
    .titleIdCount = sizeof(kTitles) / sizeof(kTitles[0]),
    .hooks        = nullptr,        // gHooks lives in another unit; see below
    .hookCount    = 0,
    .priority     = 0,
    .flags        = RPL_FLAG_ALLOW_RELEASE,
    .onInit       = OnInit,
    .onDeinit     = OnDeinit,
    .maxHooks     = 0,              // the loader default is plenty for one hook
    .onReleaseForeground  = nullptr,
    .onAcquiredForeground = nullptr,
};

} // namespace
} // namespace Cheats

// gHooks is in another unit so it cannot go in the initialiser
RPL_EXPORT const RplManifest* rpl_manifest()
{
    Cheats::gManifest.hooks     = Cheats::gHooks;
    Cheats::gManifest.hookCount = Cheats::gHookCount;
    return &Cheats::gManifest;
}

RPL_EXPORT int rpl_entry(OSDynLoad_Module module, OSDynLoad_EntryReason reason)
{
    (void)module;
    OSReport("[wwhd_cheats] rpl_entry(reason %d)\n", int(reason));
    return 0;
}

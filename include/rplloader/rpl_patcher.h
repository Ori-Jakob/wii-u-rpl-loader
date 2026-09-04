#pragma once

#include <stdint.h>

#include <libwupatch/wupatch.h>
#include <rplloader/rplloader.h>

namespace Rpl {
namespace Patcher {

// What a manifest gets without asking, and the most any manifest can ask for:
// past libwupatch's own table one RPL would be able to starve every other.
static const int kDefaultHooksPerModule = 32;
static const int kMaxHooksPerModule     = WuPatch::kMaxPatches;

struct HookSlot {
    const RplHook* hook;
    WuPatch::Handle handle;
    bool            dynamic;   // added from onInit, not from the table
};

struct ModulePatches {
    HookSlot* hooks;
    int       capacity;
    int       count;
    int32_t   priority;
    char      owner[32];
};

// Room for `want` hooks, or the default when it is 0. Clamped to what
// libwupatch can serve, which is logged. False only if the allocation failed.
bool Reserve(ModulePatches& mp, uint32_t want);
void ReleaseStorage(ModulePatches& mp);

// libwupatch for this process, physical picks the by-address path
void ConfigureProcess(uint64_t titleId, uint32_t textDelta, uint32_t dataDelta,
                      uint32_t rpxTextAddr, uint32_t rpxTextSize, bool physical);

// Declare and enable every hook then tick once, per-hook state read separately
bool DeclareTable(ModulePatches& mp, const RplManifest* m);

// First required hook that is not applied, NULL when all are
const HookSlot* FirstRequiredFailure(const ModulePatches& mp);

// Writes each applied hook's thunk into *hook->original
void Publish(const ModulePatches& mp);

// Disable, tick, undeclare; leaves nothing of this RPL in libwupatch
void Rollback(ModulePatches& mp);

// One more hook now, negative on failure
int  AddDynamic(ModulePatches& mp, const RplHook* h);
bool RemoveDynamic(ModulePatches& mp, const RplHook* h);

WuPatch::Handle HandleOf(const ModulePatches& mp, const RplHook* h);
WuPatch::State  StateOf(const ModulePatches& mp, const RplHook* h);
uint32_t        ThunkOf(const ModulePatches& mp, const RplHook* h);
const char*     StateName(WuPatch::State s);

// Where a hook points, for a log line or the menu: a link-time address, or
// module:function for a library export, which has no address of its own until
// it is resolved.
void SiteText(const RplHook* h, char* out, int cap);

// What the guard would say about a site, without declaring it
void DryRunCheck(const RplHook* h, char* out, int cap);

// Title about to exit, then the process is gone
void RemoveAll();
void OnApplicationEnd();

} // namespace Patcher
} // namespace Rpl

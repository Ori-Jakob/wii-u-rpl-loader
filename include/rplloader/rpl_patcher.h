#pragma once

#include <stdint.h>

#include <libwupatch/wupatch.h>
#include <rplloader/rplloader.h>

namespace Rpl {
namespace Patcher {

static const int kMaxHooksPerModule = 32;

struct HookSlot {
    const RplHook* hook;
    WuPatch::Handle handle;
    bool            dynamic;   // added from onInit, not from the table
};

struct ModulePatches {
    HookSlot hooks[kMaxHooksPerModule];
    int      count;
    int32_t  priority;
    char     owner[32];
};

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

// What the guard would say about a site, without declaring it
void DryRunCheck(const RplHook* h, char* out, int cap);

// Title about to exit, then the process is gone
void RemoveAll();
void OnApplicationEnd();

} // namespace Patcher
} // namespace Rpl

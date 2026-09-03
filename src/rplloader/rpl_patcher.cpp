#include "rplloader/rpl_patcher.h"

#include "rplloader/rpl_log.h"

#include <stdio.h>
#include <string.h>

#include <coreinit/memorymap.h>

#include <libwupatch/wupatch_reloc.h>

namespace Rpl {
namespace Patcher {

static uint64_t s_titleIds[1];
static uint32_t s_textDelta, s_dataDelta, s_rpxTextAddr, s_rpxTextSize;

void ConfigureProcess(uint64_t titleId, uint32_t textDelta, uint32_t dataDelta,
                      uint32_t rpxTextAddr, uint32_t rpxTextSize, bool physical)
{
    s_titleIds[0] = titleId;
    s_textDelta = textDelta;
    s_dataDelta = dataDelta;
    s_rpxTextAddr = rpxTextAddr;
    s_rpxTextSize = rpxTextSize;

    WuPatch::Config cfg = {};
    cfg.titleIds       = s_titleIds;
    cfg.titleIdCount   = 1;
    cfg.executableName = ".rpx";
    cfg.versionMin     = 0;
    cfg.versionMax     = 0xFFFF;
    cfg.tag            = "[rpl-loader][patch]";
    cfg.textDelta      = textDelta;
    cfg.textResolved   = true;
    cfg.dataDelta      = dataDelta;
    cfg.dataResolved   = true;
    cfg.backend        = WuPatch::BACKEND_FUNCTION_PATCHER;
    cfg.fpByPhysicalAddress = physical;
    WuPatch::SetLogSink(Log::WuPatchSink);
    WuPatch::Configure(cfg);
    WuPatch::OnApplicationStart();
}

static bool toDesc(const ModulePatches& mp, const RplHook* h, WuPatch::Desc& d)
{
    memset(&d, 0, sizeof(d));
    d.owner         = mp.owner;
    d.site.linkAddr = h->linkAddr;
    d.site.expected = h->expected;
    d.hook          = h->hook;
    d.replacement   = h->replacement;
    d.priority      = mp.priority;
    d.flags         = (h->flags & RPL_HOOK_EXCLUSIVE) ? WuPatch::FLAG_EXCLUSIVE : 0;
    switch (h->shape) {
    case RPL_SHAPE_JUMP:    d.shape = WuPatch::SHAPE_JUMP;    break;
    case RPL_SHAPE_CALL:    d.shape = WuPatch::SHAPE_CALL;    break;
    case RPL_SHAPE_REWRITE: d.shape = WuPatch::SHAPE_REWRITE; break;
    default:
        Log::Printf(Log::ERROR, "%s: hook '%s' has unknown shape %u", mp.owner,
                    h->name ? h->name : "?", (unsigned)h->shape);
        return false;
    }
    return true;
}

static int slotOf(const ModulePatches& mp, const RplHook* h)
{
    for (int i = 0; i < mp.count; ++i)
        if (mp.hooks[i].hook == h)
            return i;
    return -1;
}

static bool declareOne(ModulePatches& mp, const RplHook* h, bool dynamic)
{
    if (mp.count >= kMaxHooksPerModule) {
        Log::Printf(Log::ERROR, "%s: more than %d hooks", mp.owner, kMaxHooksPerModule);
        return false;
    }
    WuPatch::Desc d;
    if (!toDesc(mp, h, d))
        return false;
    const WuPatch::Handle handle = WuPatch::Declare(d);
    if (handle == WuPatch::kInvalidHandle)
        return false;
    HookSlot& s = mp.hooks[mp.count++];
    s.hook = h;
    s.handle = handle;
    s.dynamic = dynamic;
    WuPatch::SetEnabled(handle, true);
    return true;
}

bool DeclareTable(ModulePatches& mp, const RplManifest* m)
{
    mp.count = 0;
    mp.priority = m->priority;
    bool ok = true;
    for (uint32_t i = 0; i < m->hookCount; ++i) {
        if (!declareOne(mp, &m->hooks[i], false))
            ok = false;
    }
    WuPatch::Tick();
    for (int i = 0; i < mp.count; ++i) {
        const HookSlot& s = mp.hooks[i];
        const WuPatch::State st = WuPatch::GetState(s.handle);
        Log::Printf(st == WuPatch::STATE_APPLIED ? Log::INFO : Log::WARN,
                    "%s: %-24s %08X %s%s chain %d/%d", mp.owner, s.hook->name ? s.hook->name : "?",
                    (unsigned)s.hook->linkAddr, WuPatch::StateName(st),
                    (s.hook->flags & RPL_HOOK_REQUIRED) ? " (required)" : "",
                    WuPatch::ChainPosition(s.handle), WuPatch::ChainLength(s.handle));
    }
    return ok;
}

const HookSlot* FirstRequiredFailure(const ModulePatches& mp)
{
    for (int i = 0; i < mp.count; ++i) {
        const HookSlot& s = mp.hooks[i];
        if ((s.hook->flags & RPL_HOOK_REQUIRED) &&
            WuPatch::GetState(s.handle) != WuPatch::STATE_APPLIED)
            return &s;
    }
    return 0;
}

void Publish(const ModulePatches& mp)
{
    for (int i = 0; i < mp.count; ++i) {
        const HookSlot& s = mp.hooks[i];
        if (s.hook->original)
            *s.hook->original = (void*)(uintptr_t)WuPatch::GetOriginalThunk(s.handle);
    }
}

void Rollback(ModulePatches& mp)
{
    for (int i = 0; i < mp.count; ++i)
        WuPatch::SetEnabled(mp.hooks[i].handle, false);
    WuPatch::Tick();
    for (int i = 0; i < mp.count; ++i) {
        if (mp.hooks[i].hook->original)
            *mp.hooks[i].hook->original = 0;
        WuPatch::Undeclare(mp.hooks[i].handle);
    }
    mp.count = 0;
}

int AddDynamic(ModulePatches& mp, const RplHook* h)
{
    if (!h)
        return -1;
    if (slotOf(mp, h) >= 0)
        return WuPatch::GetState(HandleOf(mp, h)) == WuPatch::STATE_APPLIED ? 0 : -2;
    if (!declareOne(mp, h, true))
        return -3;
    WuPatch::Tick();
    const HookSlot& s = mp.hooks[mp.count - 1];
    const WuPatch::State st = WuPatch::GetState(s.handle);
    if (h->original)
        *h->original = (void*)(uintptr_t)WuPatch::GetOriginalThunk(s.handle);
    Log::Printf(st == WuPatch::STATE_APPLIED ? Log::INFO : Log::WARN,
                "%s: dynamic %-16s %08X %s", mp.owner, h->name ? h->name : "?",
                (unsigned)h->linkAddr, WuPatch::StateName(st));
    if (st == WuPatch::STATE_APPLIED)
        return 0;
    // Leave nothing behind for a hook that did not take.
    WuPatch::SetEnabled(s.handle, false);
    WuPatch::Tick();
    WuPatch::Undeclare(s.handle);
    --mp.count;
    return -4;
}

bool RemoveDynamic(ModulePatches& mp, const RplHook* h)
{
    const int i = slotOf(mp, h);
    if (i < 0)
        return false;
    WuPatch::SetEnabled(mp.hooks[i].handle, false);
    WuPatch::Tick();
    if (h->original)
        *h->original = 0;
    WuPatch::Undeclare(mp.hooks[i].handle);
    for (int k = i; k + 1 < mp.count; ++k)
        mp.hooks[k] = mp.hooks[k + 1];
    --mp.count;
    return true;
}

WuPatch::Handle HandleOf(const ModulePatches& mp, const RplHook* h)
{
    const int i = slotOf(mp, h);
    return i >= 0 ? mp.hooks[i].handle : WuPatch::kInvalidHandle;
}

WuPatch::State StateOf(const ModulePatches& mp, const RplHook* h)
{
    return WuPatch::GetState(HandleOf(mp, h));
}

uint32_t ThunkOf(const ModulePatches& mp, const RplHook* h)
{
    return WuPatch::GetOriginalThunk(HandleOf(mp, h));
}

const char* StateName(WuPatch::State s)
{
    return WuPatch::StateName(s);
}

void DryRunCheck(const RplHook* h, char* out, int cap)
{
    const uint32_t runtime = h->linkAddr + s_textDelta;
    if (runtime < s_rpxTextAddr || runtime - s_rpxTextAddr + 4u > s_rpxTextSize) {
        snprintf(out, (size_t)cap, "%08X -> %08X outside the rpx text", (unsigned)h->linkAddr, (unsigned)runtime);
        return;
    }
    if ((runtime & 3u) || !OSIsAddressValid(runtime)) {
        snprintf(out, (size_t)cap, "%08X -> %08X unaligned or unmapped", (unsigned)h->linkAddr, (unsigned)runtime);
        return;
    }
    const uint32_t live = *(const volatile uint32_t*)(uintptr_t)runtime;
    const bool ok = WuPatch::Reloc::MatchesNative(live, h->expected, s_dataDelta);
    snprintf(out, (size_t)cap, "%08X -> %08X live %08X %s", (unsigned)h->linkAddr, (unsigned)runtime,
             (unsigned)live, ok ? "would apply" : "would refuse");
}

void RemoveAll()
{
    WuPatch::RemoveAll();
}

void OnApplicationEnd()
{
    WuPatch::OnApplicationEnd();
}

} // namespace Patcher
} // namespace Rpl

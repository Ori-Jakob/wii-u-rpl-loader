#include "rplloader/rpl_host.h"

#include "rplloader/rpl_config.h"
#include "rplloader/rpl_input.h"
#include "rplloader/rpl_log.h"
#include "rplloader/rpl_notify.h"
#include "rplloader/rpl_patcher.h"

#include <stdarg.h>
#include <stdio.h>

#include <memory/mappedmemory.h>

namespace Rpl {
namespace Host {

static uint64_t s_titleId;
static uint32_t s_textDelta, s_dataDelta;

static Module* moduleOf(const RplHost* h)
{
    return h ? (Module*)h->impl : 0;
}

static int levelOf(int rpliLevel)
{
    switch (rpliLevel) {
    case RPL_LOG_ERROR:   return Log::ERROR;
    case RPL_LOG_WARN:    return Log::WARN;
    case RPL_LOG_INFO:    return Log::INFO;
    default:               return Log::VERBOSE;
    }
}

static void hostLog(const RplHost* h, int level, const char* fmt, ...)
{
    Module* m = moduleOf(h);
    va_list args;
    va_start(args, fmt);
    Log::VPrintf(levelOf(level), m ? m->name : "?", fmt, args);
    va_end(args);
}

static void hostNotify(const RplHost* h, int kind, const char* text)
{
    Module* m = moduleOf(h);
    char line[128];
    snprintf(line, sizeof(line), "%s: %s", m ? m->name : "?", text ? text : "");
    if (kind == RPL_NOTIFY_ERROR)
        Notify::Error(line);
    else
        Notify::Info(line);
}

static uint32_t hostOriginal(const RplHost* h, const RplHook* hook)
{
    Module* m = moduleOf(h);
    return m ? Patcher::ThunkOf(m->patches, hook) : 0;
}

static int hostState(const RplHost* h, const RplHook* hook)
{
    Module* m = moduleOf(h);
    if (!m)
        return RPL_STATE_DECLARED;
    switch (Patcher::StateOf(m->patches, hook)) {
    case WuPatch::STATE_BLOCKED:  return RPL_STATE_BLOCKED;
    case WuPatch::STATE_REFUSED:  return RPL_STATE_REFUSED;
    case WuPatch::STATE_COLLIDED: return RPL_STATE_COLLIDED;
    case WuPatch::STATE_APPLIED:  return RPL_STATE_APPLIED;
    case WuPatch::STATE_FAILED:   return RPL_STATE_FAILED;
    default:                      return RPL_STATE_DECLARED;
    }
}

static int hostAddHook(const RplHost* h, const RplHook* hook)
{
    Module* m = moduleOf(h);
    if (!m)
        return -1;
    if (Config::Get().dryRun) {
        char line[96];
        Patcher::DryRunCheck(hook, line, sizeof(line));
        Log::Printf(Log::INFO, "%s: dry run, dynamic %s: %s", m->name, hook->name ? hook->name : "?", line);
        return -5;
    }
    return Patcher::AddDynamic(m->patches, hook);
}

static int hostRemoveHook(const RplHost* h, const RplHook* hook)
{
    Module* m = moduleOf(h);
    return m && Patcher::RemoveDynamic(m->patches, hook) ? 0 : -1;
}

static void* hostAlloc(const RplHost*, uint32_t size, uint32_t align)
{
    if (!MEMAllocFromMappedMemoryEx)
        return 0;
    if (align < 4)
        align = 4;
    return MEMAllocFromMappedMemoryEx(size, (int32_t)align);
}

static void hostFree(const RplHost*, void* p)
{
    if (p && MEMFreeToMappedMemory)
        MEMFreeToMappedMemory(p);
}

static uint64_t hostTitleId(const RplHost*)   { return s_titleId; }
static uint32_t hostTextDelta(const RplHost*) { return s_textDelta; }
static uint32_t hostDataDelta(const RplHost*) { return s_dataDelta; }

static int hostGetBool(const RplHost* h, const char* key, int def)
{
    Module* m = moduleOf(h);
    if (!m || !key)
        return def;
    return Config::GetModuleBool(s_titleId, m->entry.stem, key, def != 0) ? 1 : 0;
}

static int hostSetBool(const RplHost* h, const char* key, int value)
{
    Module* m = moduleOf(h);
    if (!m || !key)
        return -1;
    return Config::SetModuleBool(s_titleId, m->entry.stem, key, value != 0) ? 0 : -1;
}

static int hostPad(const RplHost*, RplPad* out)
{
    return Input::Get(out) ? 1 : 0;
}

static int hostKpad(const RplHost*, uint32_t chan, RplKpad* out)
{
    return Input::GetKpad(chan, out) ? 1 : 0;
}

static void hostSetInputMode(const RplHost*, int mode)
{
    Input::SetMode(mode);
}

static void hostSetStick(const RplHost*, const float* leftXY)
{
    Input::SetStick(leftXY);
}

void Bind(Module& m, uint64_t titleId, uint32_t textDelta, uint32_t dataDelta, const char* dir)
{
    s_titleId = titleId;
    s_textDelta = textDelta;
    s_dataDelta = dataDelta;

    RplHost& h = m.host;
    h.version    = RPL_ABI_VERSION;
    h.name       = m.name;
    h.dir        = dir;
    h.impl       = &m;
    h.log        = hostLog;
    h.notify     = hostNotify;
    h.original   = hostOriginal;
    h.state      = hostState;
    h.addHook    = hostAddHook;
    h.removeHook = hostRemoveHook;
    h.alloc      = hostAlloc;
    h.free       = hostFree;
    h.titleId    = hostTitleId;
    h.textDelta  = hostTextDelta;
    h.dataDelta  = hostDataDelta;
    h.getBool    = hostGetBool;
    h.setBool    = hostSetBool;
    h.pad        = hostPad;
    h.kpad       = hostKpad;
    h.setInputMode = hostSetInputMode;
    h.setStick   = hostSetStick;
}

} // namespace Host
} // namespace Rpl

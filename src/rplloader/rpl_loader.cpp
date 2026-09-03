#include "rplloader/rpl_loader.h"

#include "rplloader/rpl_build.h"
#include "rplloader/rpl_config.h"
#include "rplloader/rpl_host.h"
#include "rplloader/rpl_input.h"
#include "rplloader/rpl_linker.h"
#include "rplloader/rpl_log.h"
#include "rplloader/rpl_notify.h"
#include "rplloader/rpl_patcher.h"
#include "rplloader/rpl_scan.h"

#include <stdarg.h>
#include <stdio.h>
#include <string.h>

#include <coreinit/title.h>
#include <vpad/input.h>

#include <libwupatch/wupatch.h>

namespace Rpl {
namespace Loader {

static Module   s_modules[kMaxModules];
static int      s_count = 0;
static uint64_t s_titleId = 0;
static char     s_dir[96];
static bool     s_safeMode = false;
static bool     s_started = false;
static bool     s_exited = false;
static bool     s_configured = false;
static uint32_t s_textDelta, s_dataDelta, s_rpxText, s_rpxSize;


static void copyText(char* dst, int cap, const char* src)
{
    if (!src)
        src = "";
    strncpy(dst, src, (size_t)cap - 1);
    dst[cap - 1] = '\0';
}

static void setStatus(Module& m, ModuleStatus st, const char* fmt, ...)
    __attribute__((format(printf, 3, 4)));

static void setStatus(Module& m, ModuleStatus st, const char* fmt, ...)
{
    m.status = st;
    va_list args;
    va_start(args, fmt);
    vsnprintf(m.statusText, sizeof(m.statusText), fmt, args);
    va_end(args);
}

static bool comboHeld(int combo)
{
    uint32_t mask = 0;
    switch (combo) {
    case Config::COMBO_L_R_ZL_ZR: mask = VPAD_BUTTON_L | VPAD_BUTTON_R | VPAD_BUTTON_ZL | VPAD_BUTTON_ZR; break;
    case Config::COMBO_MINUS:     mask = VPAD_BUTTON_MINUS; break;
    case Config::COMBO_PLUS_MINUS: mask = VPAD_BUTTON_PLUS | VPAD_BUTTON_MINUS; break;
    default:                      return false;
    }
    VPADInit();
    for (int attempt = 0; attempt < 3; ++attempt) {
        VPADStatus st;
        VPADReadError err = VPAD_READ_SUCCESS;
        memset(&st, 0, sizeof(st));
        if (VPADRead(VPAD_CHAN_0, &st, 1, &err) > 0 && err == VPAD_READ_SUCCESS)
            return (st.hold & mask) == mask;
    }
    return false;
}

// Hooks out first, then quarantine or release
static void fail(Module& m, ModuleStatus st, const char* why)
{
    // Sites restored and descriptors retired before the module can be freed
    Patcher::Rollback(m.patches);

    const Config::Settings& cfg = Config::Get();
    const bool allows = m.manifest && (m.manifest->flags & RPL_FLAG_ALLOW_RELEASE);
    if (m.handle && cfg.releaseFailed && allows) {
        // If Release is not survivable the log stops between these two lines
        Log::Printf(Log::INFO, "%s: releasing", m.name);
        Linker::Release(m.handle, cfg.mappedAllocator);
        m.released = true;
        m.handle = 0;
        m.manifest = 0;
        m.textAddr = m.textSize = m.dataAddr = m.dataSize = 0;
        Log::Printf(Log::INFO, "%s: released; the title survived it", m.name);
    } else if (m.handle) {
        m.quarantined = true;
        Log::Printf(Log::INFO, "%s: quarantined, %s", m.name,
                    !allows ? "its manifest does not set RPL_FLAG_ALLOW_RELEASE"
                            : "'Release failed RPLs' is off in the config menu");
    }

    setStatus(m, st, "%s%s", why, m.released ? " (released)" : m.quarantined ? " (quarantined)" : "");
    Log::Printf(Log::ERROR, "%s: %s", m.name, m.statusText);
    char line[128];
    snprintf(line, sizeof(line), "rpl-loader: %s failed: %s", m.name, why);
    Notify::Error(line);
}

static void loadOne(Module& m)
{
    const Config::Settings& cfg = Config::Get();

    if (m.entry.nameTooLong || m.entry.stemHasDot) {
        setStatus(m, MOD_SKIPPED_NAME, m.entry.nameTooLong
                  ? "name longer than %d characters before .rpl"
                  : "name has a dot before .rpl (limit %d chars)", Scan::kMaxStem);
        Log::Printf(Log::ERROR, "%s: skipped, %s", m.entry.file, m.statusText);
        return;
    }
    if (!Config::IsModuleEnabled(s_titleId, m.entry.stem)) {
        setStatus(m, MOD_DISABLED, "disabled in the menu");
        Log::Printf(Log::INFO, "%s: %s", m.entry.file, m.statusText);
        return;
    }

    Linker::AcquireName(s_titleId, m.entry.file, m.acquireName, sizeof(m.acquireName));
    // If the second line never appears the loader hung
    Log::Printf(Log::INFO, "%s: acquiring %s (%s allocator)", m.entry.file, m.acquireName,
                cfg.mappedAllocator ? "mapped-memory" : "default");
    char err[96];
    if (!Linker::Acquire(m.acquireName, cfg.mappedAllocator, &m.handle, err, sizeof(err))) {
        setStatus(m, MOD_LOAD_FAILED, "%s", err);
        Log::Printf(Log::ERROR, "%s: %s (%s)", m.entry.file, err, m.acquireName);
        char line[128];
        snprintf(line, sizeof(line), "rpl-loader: %s did not load", m.entry.file);
        Notify::Error(line);
        return;
    }
    Log::Printf(Log::INFO, "%s: acquired, handle %p", m.entry.file, (void*)m.handle);
    const bool listed = Linker::FindRange(m.entry.file, &m.textAddr, &m.textSize, &m.dataAddr, &m.dataSize);
    if (listed)
        Log::Printf(Log::INFO, "%s: text %08X..%08X data %08X..%08X", m.entry.file,
                    (unsigned)m.textAddr, (unsigned)(m.textAddr + m.textSize),
                    (unsigned)m.dataAddr, (unsigned)(m.dataAddr + m.dataSize));
    // Without rplname.py the loader lists it with no name and no hook can be applied
    const int unnamed = Linker::LogRplList(m.entry.file);
    if (!listed && unnamed) {
        Log::Printf(Log::ERROR, "%s: carries no file-info name, so the loader lists it as NULL "
                    "and no hook can be applied while it is loaded. Build it through "
                    "tools/rplname.py (the example Makefiles do).", m.entry.file);
        char line[128];
        snprintf(line, sizeof(line), "rpl-loader: %s has no file-info name; hooks refused", m.entry.file);
        Notify::Error(line);
    }

    if (!Linker::FindManifest(m.handle, &m.manifest, err, sizeof(err))) {
        fail(m, strncmp(err, "no ", 3) == 0 ? MOD_NO_MANIFEST : MOD_BAD_MANIFEST, err);
        return;
    }
    copyText(m.name, sizeof(m.name), m.manifest->name);
    copyText(m.version, sizeof(m.version), m.manifest->version);
    copyText(m.author, sizeof(m.author), m.manifest->author);
    Log::Printf(Log::INFO, "%s: %s %s by %s, %u hook(s), priority %d", m.entry.file, m.name,
                m.version, m.author, (unsigned)m.manifest->hookCount, (int)m.manifest->priority);

    if (m.manifest->titleIds && m.manifest->titleIdCount) {
        bool listed = false;
        for (uint32_t i = 0; i < m.manifest->titleIdCount; ++i)
            if (m.manifest->titleIds[i] == s_titleId)
                listed = true;
        if (!listed) {
            fail(m, MOD_TITLE_GATE, "manifest does not list this title");
            return;
        }
    }

    copyText(m.patches.owner, sizeof(m.patches.owner), m.name);
    Host::Bind(m, s_titleId, s_textDelta, s_dataDelta, s_dir);

    if (cfg.dryRun) {
        for (uint32_t i = 0; i < m.manifest->hookCount; ++i) {
            char line[96];
            Patcher::DryRunCheck(&m.manifest->hooks[i], line, sizeof(line));
            const RplHook& h = m.manifest->hooks[i];
            Log::Printf(Log::INFO, "%s: dry run %-16s %s%s", m.name, h.name ? h.name : "?", line,
                        (h.flags & RPL_HOOK_REQUIRED) ? " (required)" : "");
        }
        setStatus(m, MOD_DRY_RUN, "dry run: %u hook(s) checked, nothing applied", (unsigned)m.manifest->hookCount);
        return;
    }

    Log::Printf(Log::INFO, "%s: declaring %u hook(s)", m.name, (unsigned)m.manifest->hookCount);
    Patcher::DeclareTable(m.patches, m.manifest);
    const Patcher::HookSlot* bad = Patcher::FirstRequiredFailure(m.patches);
    if (bad) {
        char why[96];
        snprintf(why, sizeof(why), "required hook '%s' at %08X is %s",
                 bad->hook->name ? bad->hook->name : "?", (unsigned)bad->hook->linkAddr,
                 Patcher::StateName(WuPatch::GetState(bad->handle)));
        fail(m, MOD_HOOKS_FAILED, why);
        return;
    }
    Patcher::Publish(m.patches);

    Log::Printf(Log::INFO, "%s: calling onInit", m.name);
    const int rc = m.manifest->onInit(&m.host);
    Log::Printf(Log::INFO, "%s: onInit returned %d", m.name, rc);
    if (rc != 0) {
        char why[64];
        snprintf(why, sizeof(why), "onInit returned %d", rc);
        fail(m, MOD_INIT_FAILED, why);
        return;
    }
    m.initialised = true;

    int applied = 0;
    for (int i = 0; i < m.patches.count; ++i)
        if (WuPatch::GetState(m.patches.hooks[i].handle) == WuPatch::STATE_APPLIED)
            ++applied;
    setStatus(m, MOD_ACTIVE, "active: %d of %d hook(s) applied", applied, m.patches.count);
    Log::Printf(Log::INFO, "%s: %s", m.name, m.statusText);
}


void OnApplicationStart()
{
    s_started = true;
    s_exited = false;
    s_safeMode = false;
    s_count = 0;
    memset(s_modules, 0, sizeof(s_modules));
    Input::Reset();

    Config::Load();
    s_titleId = OSGetTitleID();
    Scan::TitleDir(s_titleId, s_dir, sizeof(s_dir));
    // Aroma can keep the plugin it loaded at boot after the .wps is replaced
    Log::Printf(Log::INFO, "build %s", BuildStamp());

    const Config::Settings& cfg = Config::Get();
    if (!cfg.enabled) {
        Log::Printf(Log::INFO, "title %016llX: injection disabled", (unsigned long long)s_titleId);
        return;
    }

    Scan::Entry entries[kMaxModules];
    const int n = Scan::ScanDir(s_dir, entries, kMaxModules);
    Log::Printf(n ? Log::INFO : Log::VERBOSE, "title %016llX: %d rpl(s) in %s",
                (unsigned long long)s_titleId, n, s_dir);
    if (n == 0)
        return;

    for (int i = 0; i < n; ++i) {
        Module& m = s_modules[s_count++];
        m.used = true;
        m.entry = entries[i];
        copyText(m.name, sizeof(m.name), m.entry.stem);
    }

    s_safeMode = comboHeld(cfg.safeCombo);
    if (s_safeMode) {
        for (int i = 0; i < s_count; ++i)
            setStatus(s_modules[i], MOD_DISABLED, "safe mode held at launch");
        Log::Printf(Log::WARN, "safe mode held: nothing injected");
        Notify::Info("rpl-loader: safe mode, nothing injected");
        return;
    }

    if (!Linker::ResolveRpx(&s_textDelta, &s_dataDelta, &s_rpxText, &s_rpxSize)) {
        Log::Printf(Log::ERROR, "no .rpx in OSDynLoad_GetRPLInfo; nothing injected");
        Notify::Error("rpl-loader: could not find the title's rpx");
        return;
    }
    Patcher::ConfigureProcess(s_titleId, s_textDelta, s_dataDelta, s_rpxText, s_rpxSize, cfg.physicalPatch);
    s_configured = true;
    Log::Printf(Log::INFO, "patches by %s address", cfg.physicalPatch ? "physical" : "executable");

    int active = 0, failed = 0, other = 0;
    for (int i = 0; i < s_count; ++i) {
        Module& m = s_modules[i];
        loadOne(m);
        switch (m.status) {
        case MOD_ACTIVE:
            ++active;
            break;
        case MOD_LOAD_FAILED: case MOD_NO_MANIFEST: case MOD_BAD_MANIFEST:
        case MOD_HOOKS_FAILED: case MOD_INIT_FAILED:
            ++failed;
            break;
        default:
            ++other;
            break;
        }
    }

    // Dump the chain when more than one hook shares a site
    bool chained = false;
    for (int i = 0; i < s_count && !chained; ++i)
        for (int k = 0; k < s_modules[i].patches.count; ++k)
            if (WuPatch::ChainLength(s_modules[i].patches.hooks[k].handle) > 1) {
                chained = true;
                break;
            }
    if (chained || Log::GetLevel() >= Log::VERBOSE)
        WuPatch::LogState();

    char line[96];
    if (cfg.dryRun)
        snprintf(line, sizeof(line), "rpl-loader: dry run, %d rpl(s) checked", s_count);
    else
        snprintf(line, sizeof(line), "rpl-loader: %d active, %d failed, %d skipped", active, failed, other);
    if (failed)
        Notify::Error(line);
    else
        Notify::Info(line);
}

void OnApplicationExit()
{
    if (!s_started || s_exited)
        return;
    s_exited = true;
    for (int i = 0; i < s_count; ++i) {
        Module& m = s_modules[i];
        if (m.initialised && m.manifest && m.manifest->onDeinit)
            m.manifest->onDeinit();
        m.initialised = false;
    }
    if (s_configured)
        Patcher::RemoveAll();
}

void OnApplicationEnd()
{
    OnApplicationExit();
    if (s_configured)
        Patcher::OnApplicationEnd();
    s_configured = false;
    s_started = false;
}


uint64_t      TitleId()          { return s_titleId; }
const char*   TitleDir()         { return s_dir; }
int           ModuleCount()      { return s_count; }
const Module* ModuleAt(int i)    { return (i >= 0 && i < s_count) ? &s_modules[i] : 0; }
bool          SafeModeHeld()     { return s_safeMode; }

const char* StatusName(ModuleStatus s)
{
    switch (s) {
    case MOD_NONE:         return "pending";
    case MOD_SKIPPED_NAME: return "bad name";
    case MOD_DISABLED:     return "disabled";
    case MOD_LOAD_FAILED:  return "load failed";
    case MOD_NO_MANIFEST:  return "no manifest";
    case MOD_BAD_MANIFEST: return "bad manifest";
    case MOD_TITLE_GATE:   return "other title";
    case MOD_HOOKS_FAILED: return "hooks failed";
    case MOD_INIT_FAILED:  return "init failed";
    case MOD_DRY_RUN:      return "dry run";
    case MOD_ACTIVE:       return "active";
    }
    return "?";
}

void LogState()
{
    Log::Printf(Log::INFO, "state: title %016llX, %d rpl(s), deltas text %08X data %08X",
                (unsigned long long)s_titleId, s_count, (unsigned)s_textDelta, (unsigned)s_dataDelta);
    for (int i = 0; i < s_count; ++i) {
        const Module& m = s_modules[i];
        Log::Printf(Log::INFO, "  %-26s %-12s text %08X..%08X data %08X..%08X  %s", m.entry.file,
                    StatusName(m.status), (unsigned)m.textAddr, (unsigned)(m.textAddr + m.textSize),
                    (unsigned)m.dataAddr, (unsigned)(m.dataAddr + m.dataSize), m.statusText);
    }
    if (s_configured)
        WuPatch::LogState();
}

} // namespace Loader
} // namespace Rpl

#include "rplloader/rpl_config.h"

#include "rplloader/rpl_loader.h"
#include "rplloader/rpl_log.h"
#include "rplloader/rpl_notify.h"
#include "rplloader/rpl_patcher.h"

#include <stdio.h>
#include <string.h>

#include <wups.h>
#include <wups/config/WUPSConfigItemBoolean.h>
#include <wups/config/WUPSConfigItemMultipleValues.h>
#include <wups/config/WUPSConfigItemStub.h>
#include <wups/config_api.h>
#include <wups/storage.h>

namespace Rpl {
namespace Config {

static Settings s_settings;

static const char* kEnabled         = "enabled";
static const char* kNotify          = "notify";
static const char* kLogLevel        = "log_level";
static const char* kDryRun          = "dry_run";
static const char* kReleaseFailed   = "release_failed";
static const char* kMappedAllocator = "mapped_allocator";
static const char* kPhysicalPatch   = "physical_patch";
static const char* kFileLog         = "file_log";
static const char* kSafeCombo       = "safe_combo";


static bool getBool(wups_storage_item parent, const char* key, bool def)
{
    bool v = def;
    if (WUPSStorageAPI_GetBool(parent, key, &v) != WUPS_STORAGE_ERROR_SUCCESS) {
        WUPSStorageAPI_StoreBool(parent, key, def);
        v = def;
    }
    return v;
}

static int getInt(wups_storage_item parent, const char* key, int def)
{
    int32_t v = def;
    if (WUPSStorageAPI_GetInt(parent, key, &v) != WUPS_STORAGE_ERROR_SUCCESS) {
        WUPSStorageAPI_StoreInt(parent, key, def);
        v = def;
    }
    return (int)v;
}

static void apply()
{
    Log::SetLevel(s_settings.logLevel);
    Log::SetFileLogging(s_settings.fileLog);
    Notify::SetEnabled(s_settings.notify);
}

void Load()
{
    s_settings.enabled         = getBool(nullptr, kEnabled, true);
    s_settings.notify          = getBool(nullptr, kNotify, true);
    s_settings.logLevel        = getInt(nullptr, kLogLevel, Log::INFO);
    s_settings.dryRun          = getBool(nullptr, kDryRun, false);
    // A failed RPL is unlinked before onInit so it cannot have left anything running
    s_settings.releaseFailed   = getBool(nullptr, kReleaseFailed, true);
    // RPL data lands at 0x80xxxxxx instead of the title's heap
    s_settings.mappedAllocator = getBool(nullptr, kMappedAllocator, true);
    // By name walks the module list and dies on a NULL name, by address is never gated
    s_settings.physicalPatch   = getBool(nullptr, kPhysicalPatch, false);
    // On by default: the lines worth reading are usually from a session that
    // has already ended badly, and OSReport needs something listening.
    s_settings.fileLog         = getBool(nullptr, kFileLog, true);
    s_settings.safeCombo       = getInt(nullptr, kSafeCombo, COMBO_L_R_ZL_ZR);
    apply();
}

const Settings& Get() { return s_settings; }

static bool titleItem(uint64_t titleId, wups_storage_item* out, bool create)
{
    char key[24];
    snprintf(key, sizeof(key), "t%016llX", (unsigned long long)titleId);
    if (WUPSStorageAPI_GetSubItem(nullptr, key, out) == WUPS_STORAGE_ERROR_SUCCESS)
        return true;
    return create && WUPSStorageAPI_CreateSubItem(nullptr, key, out) == WUPS_STORAGE_ERROR_SUCCESS;
}

static bool moduleItem(uint64_t titleId, const char* stem, wups_storage_item* out, bool create)
{
    wups_storage_item title = nullptr;
    if (!titleItem(titleId, &title, create))
        return false;
    if (WUPSStorageAPI_GetSubItem(title, stem, out) == WUPS_STORAGE_ERROR_SUCCESS)
        return true;
    return create && WUPSStorageAPI_CreateSubItem(title, stem, out) == WUPS_STORAGE_ERROR_SUCCESS;
}

bool IsModuleEnabled(uint64_t titleId, const char* stem)
{
    wups_storage_item item = nullptr;
    if (!moduleItem(titleId, stem, &item, true))
        return true;
    return getBool(item, kEnabled, true);
}

void SetModuleEnabled(uint64_t titleId, const char* stem, bool enabled)
{
    wups_storage_item item = nullptr;
    if (moduleItem(titleId, stem, &item, true))
        WUPSStorageAPI_StoreBool(item, kEnabled, enabled);
}

bool GetModuleBool(uint64_t titleId, const char* stem, const char* key, bool def)
{
    wups_storage_item item = nullptr;
    if (!moduleItem(titleId, stem, &item, true))
        return def;
    return getBool(item, key, def);
}

bool SetModuleBool(uint64_t titleId, const char* stem, const char* key, bool value)
{
    wups_storage_item item = nullptr;
    if (!moduleItem(titleId, stem, &item, true))
        return false;
    if (WUPSStorageAPI_StoreBool(item, key, value) != WUPS_STORAGE_ERROR_SUCCESS)
        return false;
    WUPSStorageAPI_SaveStorage(false);
    return true;
}


static char s_moduleIds[kMaxModules][Scan::kNameChars + 8];

static void boolChanged(ConfigItemBoolean* item, bool value)
{
    const char* id = item->identifier;
    if (!id)
        return;
    if (strncmp(id, "m:", 2) == 0) {
        SetModuleEnabled(Loader::TitleId(), id + 2, value);
        return;
    }
    if (strcmp(id, "dump") == 0) {
        if (value)
            Loader::LogState();
        return;
    }
    if (strcmp(id, kEnabled) == 0)              s_settings.enabled = value;
    else if (strcmp(id, kNotify) == 0)          s_settings.notify = value;
    else if (strcmp(id, kDryRun) == 0)          s_settings.dryRun = value;
    else if (strcmp(id, kReleaseFailed) == 0)   s_settings.releaseFailed = value;
    else if (strcmp(id, kMappedAllocator) == 0) s_settings.mappedAllocator = value;
    else if (strcmp(id, kPhysicalPatch) == 0)   s_settings.physicalPatch = value;
    else if (strcmp(id, kFileLog) == 0)         s_settings.fileLog = value;
    else return;
    WUPSStorageAPI_StoreBool(nullptr, id, value);
    apply();
}

// Menu index to log level, the pair values have to be contiguous
static const int kLogLevels[] = { Log::OFF, Log::ERROR, Log::INFO, Log::VERBOSE };

static void logLevelChanged(ConfigItemMultipleValues*, uint32_t value)
{
    if (value >= sizeof(kLogLevels) / sizeof(kLogLevels[0]))
        return;
    s_settings.logLevel = kLogLevels[value];
    WUPSStorageAPI_StoreInt(nullptr, kLogLevel, s_settings.logLevel);
    apply();
}

static void safeComboChanged(ConfigItemMultipleValues*, uint32_t value)
{
    s_settings.safeCombo = (int)value;
    WUPSStorageAPI_StoreInt(nullptr, kSafeCombo, s_settings.safeCombo);
}

static int logLevelIndex(int level)
{
    for (unsigned i = 0; i < sizeof(kLogLevels) / sizeof(kLogLevels[0]); ++i)
        if (kLogLevels[i] == level)
            return (int)i;
    return 2;
}

static void addStub(WUPSConfigCategoryHandle cat, const char* fmt, ...)
{
    char line[96];
    va_list args;
    va_start(args, fmt);
    vsnprintf(line, sizeof(line), fmt, args);
    va_end(args);
    WUPSConfigItemStub_AddToCategory(cat, line);
}

static void addModule(WUPSConfigCategoryHandle parent, const Module& m, int index)
{
    char title[80];
    snprintf(title, sizeof(title), "%s  [%s]", m.entry.file, Loader::StatusName(m.status));
    WUPSConfigAPICreateCategoryOptionsV1 opt = { title };
    WUPSConfigCategoryHandle cat;
    if (WUPSConfigAPI_Category_Create(opt, &cat) != WUPSCONFIG_API_RESULT_SUCCESS)
        return;

    snprintf(s_moduleIds[index], sizeof(s_moduleIds[index]), "m:%s", m.entry.stem);
    WUPSConfigItemBoolean_AddToCategory(cat, s_moduleIds[index], "Enabled (next launch)", true,
                                        IsModuleEnabled(Loader::TitleId(), m.entry.stem), boolChanged);
    if (m.manifest || m.name[0])
        addStub(cat, "%s %s by %s", m.name, m.version, m.author);
    addStub(cat, "%s", m.statusText);
    if (m.textAddr)
        addStub(cat, "text %08X..%08X data %08X..%08X", (unsigned)m.textAddr,
                (unsigned)(m.textAddr + m.textSize), (unsigned)m.dataAddr,
                (unsigned)(m.dataAddr + m.dataSize));
    for (int i = 0; i < m.patches.count; ++i) {
        const Patcher::HookSlot& s = m.patches.hooks[i];
        const WuPatch::State st = WuPatch::GetState(s.handle);
        char where[64];
        Patcher::SiteText(s.hook, where, sizeof(where));
        addStub(cat, "%-14s %s %s%s %d/%d", s.hook->name ? s.hook->name : "?",
                where, Patcher::StateName(st),
                (s.hook->flags & RPL_HOOK_REQUIRED) ? "*" : "",
                WuPatch::ChainPosition(s.handle), WuPatch::ChainLength(s.handle));
    }
    WUPSConfigAPI_Category_AddCategory(parent, cat);
}

static WUPSConfigAPICallbackStatus menuOpened(WUPSConfigCategoryHandle root)
{
    WUPSConfigItemBoolean_AddToCategory(root, kEnabled, "Enable injection", true, s_settings.enabled, boolChanged);
    WUPSConfigItemBoolean_AddToCategory(root, kNotify, "Show notifications", true, s_settings.notify, boolChanged);

    static ConfigItemMultipleValuesPair levels[] = {
        { 0, "off" }, { 1, "errors" }, { 2, "info" }, { 3, "verbose" },
    };
    WUPSConfigItemMultipleValues_AddToCategory(root, kLogLevel, "Log level", 2, logLevelIndex(s_settings.logLevel),
                                               levels, 4, logLevelChanged);

    WUPSConfigItemBoolean_AddToCategory(root, kDryRun, "Dry run (check, apply nothing)", false, s_settings.dryRun, boolChanged);
    WUPSConfigItemBoolean_AddToCategory(root, kReleaseFailed, "Release failed RPLs (needs manifest flag)", true,
                                        s_settings.releaseFailed, boolChanged);
    WUPSConfigItemBoolean_AddToCategory(root, kMappedAllocator, "RPL data in mapped memory", true,
                                        s_settings.mappedAllocator, boolChanged);
    WUPSConfigItemBoolean_AddToCategory(root, kPhysicalPatch, "Patch by physical address (experimental)", false,
                                        s_settings.physicalPatch, boolChanged);
    WUPSConfigItemBoolean_AddToCategory(root, kFileLog, "Write a log file to sd:/wiiu/rpl-loader/logs", true,
                                        s_settings.fileLog, boolChanged);

    static ConfigItemMultipleValuesPair combos[] = {
        { COMBO_NONE, "none" }, { COMBO_L_R_ZL_ZR, "L+R+ZL+ZR" }, { COMBO_MINUS, "Minus" }, { COMBO_PLUS_MINUS, "Plus+Minus" },
    };
    WUPSConfigItemMultipleValues_AddToCategory(root, kSafeCombo, "Safe mode: hold at launch", COMBO_L_R_ZL_ZR,
                                               s_settings.safeCombo, combos, 4, safeComboChanged);

    char title[64];
    snprintf(title, sizeof(title), "This title: %016llX", (unsigned long long)Loader::TitleId());
    WUPSConfigAPICreateCategoryOptionsV1 opt = { title };
    WUPSConfigCategoryHandle cat;
    if (WUPSConfigAPI_Category_Create(opt, &cat) == WUPSCONFIG_API_RESULT_SUCCESS) {
        if (Loader::SafeModeHeld())
            addStub(cat, "Safe mode was held: nothing injected");
        const int n = Loader::ModuleCount();
        if (n == 0)
            addStub(cat, "No .rpl files in %s", Loader::TitleDir());
        for (int i = 0; i < n; ++i)
            addModule(cat, *Loader::ModuleAt(i), i);
        WUPSConfigAPI_Category_AddCategory(root, cat);
    }

    WUPSConfigItemBoolean_AddToCategory(root, "dump", "Dump state to log (toggle)", false, false, boolChanged);
    return WUPSCONFIG_API_CALLBACK_RESULT_SUCCESS;
}

static void menuClosed()
{
    WUPSStorageAPI_SaveStorage(false);
}

void InitMenu()
{
    WUPSConfigAPIOptionsV1 opt = { "RPL Loader" };
    const WUPSConfigAPIStatus st = WUPSConfigAPI_Init(opt, menuOpened, menuClosed);
    if (st != WUPSCONFIG_API_RESULT_SUCCESS)
        Log::Printf(Log::ERROR, "WUPSConfigAPI_Init failed: %s", WUPSConfigAPI_GetStatusStr(st));
}

} // namespace Config
} // namespace Rpl

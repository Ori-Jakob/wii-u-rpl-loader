#pragma once

#include <stdint.h>

namespace Rpl {
namespace Config {

enum SafeCombo {
    COMBO_NONE = 0,
    COMBO_L_R_ZL_ZR,
    COMBO_MINUS,
    COMBO_PLUS_MINUS,
};

struct Settings {
    bool enabled;          // inject at all
    bool notify;           // on-screen notifications
    int  logLevel;         // Log::Level
    bool dryRun;           // load and check, apply nothing
    bool releaseFailed;    // OSDynLoad_Release a failed RPL that allows it
    bool mappedAllocator;  // give the loader MemoryMappingModule memory for RPL data
    bool fileLog;          // mirror the log to sd:/wiiu/rpl-loader/logs/
    bool physicalPatch;    // libwupatch: FunctionPatcher by physical address, not by executable name
    int  safeCombo;        // SafeCombo
};

// Reads settings from WUPS storage, writing defaults for anything missing
void Load();
const Settings& Get();

// Per-title, per-RPL switches
bool IsModuleEnabled(uint64_t titleId, const char* stem);
void SetModuleEnabled(uint64_t titleId, const char* stem, bool enabled);

// An RPL's own slice, via the host API
bool GetModuleBool(uint64_t titleId, const char* stem, const char* key, bool def);
bool SetModuleBool(uint64_t titleId, const char* stem, const char* key, bool value);
int32_t GetModuleInt(uint64_t titleId, const char* stem, const char* key, int32_t def);
bool SetModuleInt(uint64_t titleId, const char* stem, const char* key, int32_t value);

// Once, from INITIALIZE_PLUGIN
void InitMenu();

} // namespace Config
} // namespace Rpl

#pragma once

#include <stdint.h>

#include "rplloader/rpl_module.h"

namespace Rpl {
namespace Loader {

// Everything, in order, for the running title
void OnApplicationStart();

// onDeinit for each initialised RPL then RemoveAll, idempotent
void OnApplicationExit();
void OnReleaseForeground();
void OnAcquiredForeground();
// Process is gone
void OnApplicationEnd();

// For the menu
uint64_t      TitleId();
const char*   TitleDir();
int           ModuleCount();
const Module* ModuleAt(int index);
bool          SafeModeHeld();   // this boot skipped injection because of the combo
const char*   StatusName(ModuleStatus s);

// Dumps patch state and module ranges to the log
void LogState();

} // namespace Loader
} // namespace Rpl

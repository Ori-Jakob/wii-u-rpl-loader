#pragma once

#include <stdint.h>

#include <coreinit/dynload.h>

#include "rplloader/rpl_patcher.h"
#include "rplloader/rpl_scan.h"
#include <rplloader/rplloader.h>

namespace Rpl {

static const int kMaxModules = Scan::kMaxEntries;
static const int kTextChars  = 32;

enum ModuleStatus {
    MOD_NONE = 0,
    MOD_SKIPPED_NAME,    // file name breaks a loader rule
    MOD_DISABLED,        // switched off in the menu
    MOD_LOAD_FAILED,     // OSDynLoad_Acquire refused
    MOD_NO_MANIFEST,     // not a loader RPL
    MOD_BAD_MANIFEST,    // magic / ABI / table
    MOD_TITLE_GATE,      // manifest lists titles and this is not one
    MOD_HOOKS_FAILED,    // a required hook could not be applied
    MOD_INIT_FAILED,     // onInit returned nonzero
    MOD_DRY_RUN,         // loaded and checked, nothing applied
    MOD_ACTIVE,
};

struct Module {
    bool          used;
    Scan::Entry   entry;
    char          name[kTextChars];      // manifest name, or the stem
    char          version[kTextChars];
    char          author[kTextChars];
    char          acquireName[64];       // "~/wiiu/rpl-loader/<tid>/<file>"

    OSDynLoad_Module   handle;
    const RplManifest* manifest;
    uint32_t      textAddr, textSize;    // where the loader put it, for crash attribution
    uint32_t      dataAddr, dataSize;

    Patcher::ModulePatches patches;
    RplHost      host;

    ModuleStatus  status;
    char          statusText[96];
    bool          initialised;           // onInit ran and returned 0
    bool          quarantined;           // left resident after a failure
    bool          released;              // OSDynLoad_Release was called
};

} // namespace Rpl

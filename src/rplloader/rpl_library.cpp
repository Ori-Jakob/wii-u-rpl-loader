#include "rplloader/rpl_library.h"

#include "rplloader/rpl_log.h"

#include <string.h>

#include <coreinit/dynload.h>
#include <function_patcher/fpatching_defines.h>

#include <libwupatch/wupatch_types.h>

namespace Rpl {
namespace Library {

namespace {

struct Entry {
    const char* module;
    uint32_t    id;
};

// The same set the FunctionPatcher module carries, minus the .rpl, so an RPL
// can name a library the way everyone writes it.
const Entry kLibraries[] = {
    { "avm",       LIBRARY_AVM },       { "camera",   LIBRARY_CAMERA },
    { "coreinit",  LIBRARY_COREINIT },  { "dc",       LIBRARY_DC },
    { "dmae",      LIBRARY_DMAE },      { "drmapp",   LIBRARY_DRMAPP },
    { "erreula",   LIBRARY_ERREULA },   { "gx2",      LIBRARY_GX2 },
    { "h264",      LIBRARY_H264 },      { "lzma920",  LIBRARY_LZMA920 },
    { "mic",       LIBRARY_MIC },       { "nfc",      LIBRARY_NFC },
    { "nio_prof",  LIBRARY_NIO_PROF },  { "nlibcurl", LIBRARY_NLIBCURL },
    { "nlibnss",   LIBRARY_NLIBNSS },   { "nlibnss2", LIBRARY_NLIBNSS2 },
    { "nn_ac",     LIBRARY_NN_AC },     { "nn_acp",   LIBRARY_NN_ACP },
    { "nn_act",    LIBRARY_NN_ACT },    { "nn_aoc",   LIBRARY_NN_AOC },
    { "nn_boss",   LIBRARY_NN_BOSS },   { "nn_ccr",   LIBRARY_NN_CCR },
    { "nn_cmpt",   LIBRARY_NN_CMPT },   { "nn_dlp",   LIBRARY_NN_DLP },
    { "nn_ec",     LIBRARY_NN_EC },     { "nn_fp",    LIBRARY_NN_FP },
    { "nn_hai",    LIBRARY_NN_HAI },    { "nn_hpad",  LIBRARY_NN_HPAD },
    { "nn_idbe",   LIBRARY_NN_IDBE },   { "nn_ndm",   LIBRARY_NN_NDM },
    { "nn_nets2",  LIBRARY_NN_NETS2 },  { "nn_nfp",   LIBRARY_NN_NFP },
    { "nn_nim",    LIBRARY_NN_NIM },    { "nn_olv",   LIBRARY_NN_OLV },
    { "nn_pdm",    LIBRARY_NN_PDM },    { "nn_save",  LIBRARY_NN_SAVE },
    { "nn_sl",     LIBRARY_NN_SL },     { "nn_spm",   LIBRARY_NN_SPM },
    { "nn_temp",   LIBRARY_NN_TEMP },   { "nn_uds",   LIBRARY_NN_UDS },
    { "nn_vctl",   LIBRARY_NN_VCTL },   { "nsysccr",  LIBRARY_NSYSCCR },
    { "nsyshid",   LIBRARY_NSYSHID },   { "nsyskbd",  LIBRARY_NSYSKBD },
    { "nsysnet",   LIBRARY_NSYSNET },   { "nsysuhs",  LIBRARY_NSYSUHS },
    { "nsysuvd",   LIBRARY_NSYSUVD },   { "ntag",     LIBRARY_NTAG },
    { "padscore",  LIBRARY_PADSCORE },  { "proc_ui",  LIBRARY_PROC_UI },
    { "snd_core",  LIBRARY_SND_CORE },  { "snd_user", LIBRARY_SND_USER },
    { "sndcore2",  LIBRARY_SNDCORE2 },  { "snduser2", LIBRARY_SNDUSER2 },
    { "swkbd",     LIBRARY_SWKBD },     { "sysapp",   LIBRARY_SYSAPP },
    { "tcl",       LIBRARY_TCL },       { "tve",      LIBRARY_TVE },
    { "uac",       LIBRARY_UAC },       { "uac_rpl",  LIBRARY_UAC_RPL },
    { "usb_mic",   LIBRARY_USB_MIC },   { "uvc",      LIBRARY_UVC },
    { "uvd",       LIBRARY_UVD },       { "vpad",     LIBRARY_VPAD },
    { "vpadbase",  LIBRARY_VPADBASE },  { "zlib125",  LIBRARY_ZLIB125 },
};

} // namespace

uint32_t IdFor(const char* module)
{
    if (module) {
        for (unsigned i = 0; i < sizeof(kLibraries) / sizeof(kLibraries[0]); ++i)
            if (strcmp(kLibraries[i].module, module) == 0)
                return kLibraries[i].id;
    }
    return WuPatch::kLibraryByAddress;
}

uint32_t Resolve(const char* module, const char* function)
{
    if (!module || !function)
        return 0;

    OSDynLoad_Module handle = 0;
    if (OSDynLoad_Acquire(module, &handle) != OS_DYNLOAD_OK || !handle) {
        Log::Printf(Log::WARN, "%s is not loaded", module);
        return 0;
    }
    void* addr = 0;
    const OSDynLoad_Error rc =
        OSDynLoad_FindExport(handle, OS_DYNLOAD_EXPORT_FUNC, function, &addr);
    // The title holds its own reference; ours was only to look the name up
    OSDynLoad_Release(handle);

    if (rc != OS_DYNLOAD_OK || !addr) {
        Log::Printf(Log::WARN, "%s does not export %s", module, function);
        return 0;
    }
    return (uint32_t)(uintptr_t)addr;
}

} // namespace Library
} // namespace Rpl

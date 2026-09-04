#ifndef CHAIN_NAME
#error "define CHAIN_NAME before including chain_common.h"
#endif
#ifndef CHAIN_PRIORITY
#error "define CHAIN_PRIORITY before including chain_common.h"
#endif
#ifndef CHAIN_EXCLUSIVE
#define CHAIN_EXCLUSIVE 0
#endif

#if CHAIN_EXCLUSIVE
#define CHAIN_HOOK_FLAGS (RPL_HOOK_REQUIRED | RPL_HOOK_EXCLUSIVE)
#define CHAIN_MODE       "exclusive"
#else
#define CHAIN_HOOK_FLAGS RPL_HOOK_REQUIRED
#define CHAIN_MODE       "shared"
#endif

#include <coreinit/debug.h>
#include <coreinit/dynload.h>

#include <rplloader/rplloader.h>

static const RplHost* gHost;
static uint32_t gFrames;

// real_cCt_Counter is the next link or the real function, not calling it stops the counter
RPL_DECL_REPLACE(void, cCt_Counter, int reset)
{
    if (++gFrames == 60 && gHost)
        gHost->log(gHost, RPL_LOG_INFO, "frame 60 reached this link (priority %d)",
                   (int)CHAIN_PRIORITY);
    real_cCt_Counter(reset);
}

static const RplHook sHooks[] = {
    RPL_REPLACE(cCt_Counter, 0x0200E6ECu, 0x3D401020u, CHAIN_HOOK_FLAGS),
};

static int onInit(const RplHost* host)
{
    gHost = host;
    host->log(host, RPL_LOG_INFO, "linked at priority %d, %s site; counting frames",
              (int)CHAIN_PRIORITY, CHAIN_MODE);
    return 0;
}

static void onDeinit(void)
{
    gHost = 0;
}

static const uint64_t sTitles[] = {
    0x0005000010143500ull,   /* USA BCZE */
    0x0005000010143600ull,   /* EUR BCZP */
    0x0005000010143400ull,   /* JAP BCZJ */
};

static const RplManifest sManifest = {
    RPL_MAGIC,
    RPL_ABI_VERSION,
    CHAIN_NAME,
    "0.1",
    "rpl-loader examples",
    sTitles, sizeof(sTitles) / sizeof(sTitles[0]),
    sHooks, sizeof(sHooks) / sizeof(sHooks[0]),
    CHAIN_PRIORITY,          /* higher runs earlier; ties run in load order */
    RPL_FLAG_ALLOW_RELEASE,
    onInit,
    onDeinit,
    0,                       /* maxHooks: the loader default */
    NULL, NULL,              /* no foreground callbacks */
};

RPL_MANIFEST(sManifest)

int rpl_entry(OSDynLoad_Module module, OSDynLoad_EntryReason reason)
{
    (void)module;
    OSReport("[" CHAIN_NAME "] rpl_entry(reason %d)\n", (int)reason);
    return 0;
}

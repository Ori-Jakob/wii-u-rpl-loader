#include <coreinit/debug.h>
#include <coreinit/dynload.h>

#include <rplloader/rplloader.h>

static const RplHost* gHost;

// The only lines that can still appear after the module is released
__attribute__((constructor)) static void ctor(void)
{
    OSReport("[release_test] constructor\n");
}

__attribute__((destructor)) static void dtor(void)
{
    OSReport("[release_test] destructor\n");
}

/* void JUT_ShowAssert(const char* file, int line, const char* expr) */
RPL_DECL_REPLACE(void, will_apply, const char* file, int line, const char* expr)
{
    // Only reached if an assert fires while this is live
    OSReport("[release_test] JUT_ShowAssert(%s, %d, %s)\n",
             file ? file : "?", line, expr ? expr : "?");
    real_will_apply(file, line, expr);
}

// Never entered, its site is refused
RPL_DECL_REPLACE(void, will_be_refused, const char* file, int line, const char* expr)
{
    real_will_be_refused(file, line, expr);
}

static const RplHook sHooks[] = {
    // Real site and word, applies
    RPL_REPLACE(will_apply, 0x0273AA24u, 0x7CA62B78u, RPL_HOOK_REQUIRED),
    // Wrong word on purpose so this required hook unlinks the RPL, really holds 7C0802A6
    RPL_REPLACE(will_be_refused, 0x0273AA38u, 0x9421FFE0u, RPL_HOOK_REQUIRED),
};

static int onInit(const RplHost* host)
{
    gHost = host;
    host->log(host, RPL_LOG_ERROR,
              "onInit ran, but a required hook failed -- the injector should "
              "never have got here");
    return -1;
}

static void onDeinit(void)
{
    gHost = 0;
}

// WWHD sites, so gate it to WWHD
static const uint64_t sTitles[] = {
    0x0005000010143500ull,   /* USA BCZE */
    0x0005000010143600ull,   /* EUR BCZP */
    0x0005000010143400ull,   /* JAP BCZJ */
};

static const RplManifest sManifest = {
    RPL_MAGIC,
    RPL_ABI_VERSION,
    "release_test",
    "0.1",
    "rpl-loader examples",
    sTitles, sizeof(sTitles) / sizeof(sTitles[0]),
    sHooks, sizeof(sHooks) / sizeof(sHooks[0]),
    0,                            /* priority */
    RPL_FLAG_ALLOW_RELEASE,      /* the author asserts unloading is safe */
    onInit,
    onDeinit,
};

RPL_MANIFEST(sManifest)

int rpl_entry(OSDynLoad_Module module, OSDynLoad_EntryReason reason)
{
    (void)module;
    // reason 2 is what Release runs
    OSReport("[release_test] rpl_entry(reason %d)\n", (int)reason);
    return 0;
}

#include "rplloader/rpl_linker.h"

#include "rplloader/rpl_log.h"

#include <stdio.h>
#include <string.h>
#include <strings.h>

#include <memory/mappedmemory.h>

namespace Rpl {
namespace Linker {

static const int kMaxRpls = 64;

static bool endsWith(const char* s, const char* suffix)
{
    const size_t n = strlen(s), m = strlen(suffix);
    return n >= m && strcmp(s + n - m, suffix) == 0;
}

bool ResolveRpx(uint32_t* textDelta, uint32_t* dataDelta, uint32_t* textAddr, uint32_t* textSize)
{
    const int count = OSDynLoad_GetNumberOfRPLs();
    if (count <= 0)
        return false;
    OSDynLoad_NotifyData infos[kMaxRpls];
    const int wanted = count > kMaxRpls ? kMaxRpls : count;
    if (!OSDynLoad_GetRPLInfo(0, (uint32_t)wanted, infos))
        return false;
    for (int i = 0; i < wanted; ++i) {
        if (!infos[i].name || !endsWith(infos[i].name, ".rpx"))
            continue;
        *textDelta = infos[i].textOffset;
        *dataDelta = infos[i].dataOffset;
        *textAddr = infos[i].textAddr;
        *textSize = infos[i].textSize;
        Log::Printf(Log::INFO, "rpx '%s' text=%08X+%08X (delta %08X) data=%08X+%08X (delta %08X)",
                    infos[i].name, (unsigned)infos[i].textAddr, (unsigned)infos[i].textSize,
                    (unsigned)infos[i].textOffset, (unsigned)infos[i].dataAddr,
                    (unsigned)infos[i].dataSize, (unsigned)infos[i].dataOffset);
        return true;
    }
    return false;
}

void AcquireName(const char* dirLeaf, const char* file, char* name, int cap)
{
    snprintf(name, (size_t)cap, "~/wiiu/rpl-loader/%s/%s", dirLeaf, file);
}


static OSDynLoad_Error mappedAlloc(int32_t size, int32_t align, void** outAddr)
{
    if (align < 4)
        align = 4;
    void* p = MEMAllocFromMappedMemoryEx((uint32_t)size, align);
    if (!p)
        return OS_DYNLOAD_OUT_OF_MEMORY;
    *outAddr = p;
    return OS_DYNLOAD_OK;
}

static void mappedFree(void* addr)
{
    MEMFreeToMappedMemory(addr);
}

struct AllocatorScope {
    OSDynLoadAllocFn oldAlloc;
    OSDynLoadFreeFn  oldFree;
    bool swapped;

    explicit AllocatorScope(bool wanted) : oldAlloc(0), oldFree(0), swapped(false)
    {
        if (!wanted)
            return;
        if (!MEMAllocFromMappedMemoryEx || !MEMFreeToMappedMemory) {
            Log::Printf(Log::WARN, "mapped-memory allocator requested but libmappedmemory "
                        "did not resolve in this process; using the loader's default");
            return;
        }
        if (OSDynLoad_GetAllocator(&oldAlloc, &oldFree) != OS_DYNLOAD_OK)
            return;
        if (OSDynLoad_SetAllocator(mappedAlloc, mappedFree) == OS_DYNLOAD_OK)
            swapped = true;
        Log::Printf(Log::INFO, "dynload allocator was %p/%p; %s", (void*)oldAlloc,
                    (void*)oldFree, swapped ? "swapped to mapped memory" : "swap refused");
    }
    ~AllocatorScope()
    {
        if (swapped)
            OSDynLoad_SetAllocator(oldAlloc, oldFree);
    }
};


static const char* errorName(OSDynLoad_Error e)
{
    switch (e) {
    case OS_DYNLOAD_OK:                  return "ok";
    case OS_DYNLOAD_OUT_OF_MEMORY:       return "out of memory";
    case OS_DYNLOAD_INVALID_MODULE_NAME: return "invalid module name";
    case OS_DYNLOAD_MODULE_NOT_FOUND:    return "module not found";
    default:                             return "";
    }
}

bool Acquire(const char* name, bool mappedAllocator, OSDynLoad_Module* out, char* err, int errCap)
{
    OSDynLoad_Error e;
    {
        AllocatorScope scope(mappedAllocator);
        e = OSDynLoad_Acquire(name, out);
    }
    if (e != OS_DYNLOAD_OK) {
        snprintf(err, (size_t)errCap, "OSDynLoad_Acquire: %08X %s", (unsigned)e, errorName(e));
        return false;
    }
    return true;
}

bool FindManifest(OSDynLoad_Module module, const RplManifest** out, char* err, int errCap)
{
    void* fn = 0;
    const OSDynLoad_Error e = OSDynLoad_FindExport(module, OS_DYNLOAD_EXPORT_FUNC, RPL_MANIFEST_EXPORT, &fn);
    if (e != OS_DYNLOAD_OK || !fn) {
        snprintf(err, (size_t)errCap, "no %s export (%08X)", RPL_MANIFEST_EXPORT, (unsigned)e);
        return false;
    }
    const RplManifest* m = ((RplManifestFn)fn)();
    if (!m) {
        snprintf(err, (size_t)errCap, "%s returned NULL", RPL_MANIFEST_EXPORT);
        return false;
    }
    if (m->magic != RPL_MAGIC) {
        snprintf(err, (size_t)errCap, "bad magic %08X", (unsigned)m->magic);
        return false;
    }
    if (m->abiVersion != RPL_ABI_VERSION) {
        snprintf(err, (size_t)errCap, "ABI %u, plugin speaks %u", (unsigned)m->abiVersion,
                 (unsigned)RPL_ABI_VERSION);
        return false;
    }
    if (!m->name || !m->name[0]) {
        snprintf(err, (size_t)errCap, "manifest has no name");
        return false;
    }
    if (m->hookCount && !m->hooks) {
        snprintf(err, (size_t)errCap, "hookCount %u but no table", (unsigned)m->hookCount);
        return false;
    }
    if (!m->onInit) {
        snprintf(err, (size_t)errCap, "manifest has no onInit");
        return false;
    }
    *out = m;
    return true;
}

void Release(OSDynLoad_Module module, bool mappedAllocator)
{
    AllocatorScope scope(mappedAllocator);
    OSDynLoad_Release(module);
}

int LogRplList(const char* why)
{
    const int count = OSDynLoad_GetNumberOfRPLs();
    if (count <= 0) {
        Log::Printf(Log::INFO, "rpl list (%s): none", why);
        return 0;
    }
    OSDynLoad_NotifyData infos[kMaxRpls];
    const int wanted = count > kMaxRpls ? kMaxRpls : count;
    if (!OSDynLoad_GetRPLInfo(0, (uint32_t)wanted, infos)) {
        Log::Printf(Log::WARN, "rpl list (%s): OSDynLoad_GetRPLInfo failed", why);
        return 0;
    }
    int unnamed = 0;
    Log::Printf(Log::INFO, "rpl list (%s): %d module(s)", why, count);
    for (int i = 0; i < wanted; ++i) {
        const OSDynLoad_NotifyData& n = infos[i];
        if (!n.name)
            ++unnamed;
        Log::Printf(Log::INFO, "  %2d name=%p '%s' text=%08X+%08X data=%08X+%08X", i, (void*)n.name,
                    n.name ? n.name : "<null>", (unsigned)n.textAddr, (unsigned)n.textSize,
                    (unsigned)n.dataAddr, (unsigned)n.dataSize);
    }
    return unnamed;
}

// The loader lists a module by its file-info name, so match the last path component
static const char* baseName(const char* path)
{
    const char* base = path;
    for (const char* p = path; *p; ++p)
        if (*p == '\\' || *p == '/')
            base = p + 1;
    return base;
}

bool FindRange(const char* fileName, uint32_t* textAddr, uint32_t* textSize,
               uint32_t* dataAddr, uint32_t* dataSize)
{
    const int count = OSDynLoad_GetNumberOfRPLs();
    if (count <= 0)
        return false;
    OSDynLoad_NotifyData infos[kMaxRpls];
    const int wanted = count > kMaxRpls ? kMaxRpls : count;
    if (!OSDynLoad_GetRPLInfo(0, (uint32_t)wanted, infos))
        return false;
    for (int i = 0; i < wanted; ++i) {
        if (!infos[i].name || strcasecmp(baseName(infos[i].name), fileName) != 0)
            continue;
        *textAddr = infos[i].textAddr;
        *textSize = infos[i].textSize;
        *dataAddr = infos[i].dataAddr;
        *dataSize = infos[i].dataSize;
        return true;
    }
    return false;
}

} // namespace Linker
} // namespace Rpl

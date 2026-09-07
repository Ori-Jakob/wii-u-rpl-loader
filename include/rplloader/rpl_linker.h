#pragma once

#include <stdint.h>

#include <coreinit/dynload.h>

#include <rplloader/rplloader.h>

namespace Rpl {
namespace Linker {

// Deltas and text range of the title's .rpx
bool ResolveRpx(uint32_t* textDelta, uint32_t* dataDelta, uint32_t* textAddr, uint32_t* textSize);

// Builds "~/wiiu/rpl-loader/<tid>/<file>", cap >= 64
void AcquireName(const char* dirLeaf, const char* file, char* name, int cap);

// Acquire, optionally with the dynload allocator swapped to mapped memory
bool Acquire(const char* name, bool mappedAllocator, OSDynLoad_Module* out, char* err, int errCap);

// FindExport(rpl_manifest) plus the basic checks
bool FindManifest(OSDynLoad_Module module, const RplManifest** out, char* err, int errCap);

// Release, freeing through the allocator that allocated
void Release(OSDynLoad_Module module, bool mappedAllocator);

// Matched on the file-info name rplname.py stamped in, false without it
bool FindRange(const char* fileName, uint32_t* textAddr, uint32_t* textSize,
               uint32_t* dataAddr, uint32_t* dataSize);

// Logs every loaded module, returns how many have a NULL name
int LogRplList(const char* why);

} // namespace Linker
} // namespace Rpl

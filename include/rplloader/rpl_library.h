#pragma once

#include <stdint.h>

namespace Rpl {
namespace Library {

// The patcher's id for a module name without its .rpl, so the module can
// resolve the export again in a later process. WuPatch::kLibraryByAddress for
// a name it does not know, which leaves only the address we resolved.
uint32_t IdFor(const char* module);

// Where an export landed in this process, 0 if the module is not loaded or
// has no such name.
uint32_t Resolve(const char* module, const char* function);

} // namespace Library
} // namespace Rpl

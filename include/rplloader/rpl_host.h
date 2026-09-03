#pragma once

#include "rplloader/rpl_module.h"

namespace Rpl {
namespace Host {

// Fills m.host, bound to this module
void Bind(Module& m, uint64_t titleId, uint32_t textDelta, uint32_t dataDelta, const char* dir);

} // namespace Host
} // namespace Rpl

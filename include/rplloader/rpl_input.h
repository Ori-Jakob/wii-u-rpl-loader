#pragma once

#include <rplloader/rplloader.h>

namespace Rpl {
namespace Input {

// Latest sample, false until the title has read the pad
bool Get(RplPad* out);

// New process, nothing read yet
void Reset();

} // namespace Input
} // namespace Rpl

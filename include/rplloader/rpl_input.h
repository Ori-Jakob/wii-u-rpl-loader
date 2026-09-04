#pragma once

#include <rplloader/rplloader.h>

namespace Rpl {
namespace Input {

// Latest sample, false until the title has read that controller
bool Get(RplPad* out);
bool GetKpad(uint32_t chan, RplKpad* out);

// What the title is allowed to read, and an override for its left stick
void SetMode(int mode);
void SetStick(const float* leftXY);

// New process, nothing read yet
void Reset();

} // namespace Input
} // namespace Rpl

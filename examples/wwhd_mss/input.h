#pragma once

#include <stdint.h>

#include <rplloader/rplloader.h>

namespace Mss {
namespace Input {

void Bind(const RplHost* host);
void Reset();
void Poll();

const RplPad& Pad();
bool Held(uint32_t buttons);
bool Pressed(uint32_t buttons);
bool Released(uint32_t buttons);

uint32_t WpadFromVpad(uint32_t vpad);
uint32_t VpadFromWpad(uint32_t wpad);

} // namespace Input
} // namespace Mss

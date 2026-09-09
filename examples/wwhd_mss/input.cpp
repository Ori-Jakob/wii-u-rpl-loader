#include "input.h"

#include <padscore/wpad.h>
#include <string.h>
#include <vpad/input.h>

namespace Mss {
namespace Input {
namespace {

const RplHost* sHost;
RplPad sPad;
uint32_t sPreviousHold;

const struct {
    uint32_t vpad;
    uint32_t wpad;
} kButtons[] = {
    {VPAD_BUTTON_A, WPAD_PRO_BUTTON_A},
    {VPAD_BUTTON_B, WPAD_PRO_BUTTON_B},
    {VPAD_BUTTON_X, WPAD_PRO_BUTTON_X},
    {VPAD_BUTTON_Y, WPAD_PRO_BUTTON_Y},
    {VPAD_BUTTON_LEFT, WPAD_PRO_BUTTON_LEFT},
    {VPAD_BUTTON_RIGHT, WPAD_PRO_BUTTON_RIGHT},
    {VPAD_BUTTON_UP, WPAD_PRO_BUTTON_UP},
    {VPAD_BUTTON_DOWN, WPAD_PRO_BUTTON_DOWN},
    {VPAD_BUTTON_ZL, WPAD_PRO_BUTTON_ZL},
    {VPAD_BUTTON_ZR, WPAD_PRO_BUTTON_ZR},
    {VPAD_BUTTON_L, WPAD_PRO_BUTTON_L},
    {VPAD_BUTTON_R, WPAD_PRO_BUTTON_R},
    {VPAD_BUTTON_PLUS, WPAD_PRO_BUTTON_PLUS},
    {VPAD_BUTTON_MINUS, WPAD_PRO_BUTTON_MINUS},
    {VPAD_BUTTON_HOME, WPAD_PRO_BUTTON_HOME},
    {VPAD_BUTTON_STICK_L, WPAD_PRO_BUTTON_STICK_L},
    {VPAD_BUTTON_STICK_R, WPAD_PRO_BUTTON_STICK_R},
};

bool HasButtons(uint32_t extension)
{
    return extension == WPAD_EXT_PRO_CONTROLLER ||
           extension == WPAD_EXT_CLASSIC ||
           extension == WPAD_EXT_MPLUS_CLASSIC;
}

float StickMagnitude(float lx, float ly, float rx, float ry)
{
    return lx * lx + ly * ly + rx * rx + ry * ry;
}

} // namespace

void Bind(const RplHost* host)
{
    sHost = host;
    Reset();
}

void Reset()
{
    memset(&sPad, 0, sizeof(sPad));
    sPreviousHold = 0;
}

void Poll()
{
    sPreviousHold = sPad.hold;

    RplPad merged = {};
    merged.sample = sPad.sample + 1;
    float sticks = -1.0f;

    RplPad gamepad;
    if (sHost && sHost->pad && sHost->pad(sHost, &gamepad)) {
        merged.hold = gamepad.hold;
        merged.leftX = gamepad.leftX;
        merged.leftY = gamepad.leftY;
        merged.rightX = gamepad.rightX;
        merged.rightY = gamepad.rightY;
        merged.touch = gamepad.touch;
        sticks = StickMagnitude(gamepad.leftX, gamepad.leftY,
                                gamepad.rightX, gamepad.rightY);
    }

    for (uint32_t chan = 0; sHost && sHost->kpad && chan < RPL_KPAD_CHANNELS;
         ++chan) {
        RplKpad kpad;
        if (!sHost->kpad(sHost, chan, &kpad) || !HasButtons(kpad.extension))
            continue;
        merged.hold |= VpadFromWpad(kpad.hold);
        const float magnitude =
            StickMagnitude(kpad.leftX, kpad.leftY, kpad.rightX, kpad.rightY);
        if (magnitude > sticks) {
            merged.leftX = kpad.leftX;
            merged.leftY = kpad.leftY;
            merged.rightX = kpad.rightX;
            merged.rightY = kpad.rightY;
            sticks = magnitude;
        }
    }

    merged.trigger = merged.hold & ~sPreviousHold;
    merged.release = sPreviousHold & ~merged.hold;
    sPad = merged;
}

const RplPad& Pad()
{
    return sPad;
}

bool Held(uint32_t buttons)
{
    return (sPad.hold & buttons) == buttons;
}

bool Pressed(uint32_t buttons)
{
    return Held(buttons) && (sPad.trigger & buttons) != 0;
}

bool Released(uint32_t buttons)
{
    return (sPreviousHold & buttons) == buttons && !Held(buttons);
}

uint32_t WpadFromVpad(uint32_t vpad)
{
    uint32_t wpad = 0;
    for (const auto& button : kButtons) {
        if (vpad & button.vpad)
            wpad |= button.wpad;
    }
    return wpad;
}

uint32_t VpadFromWpad(uint32_t wpad)
{
    uint32_t vpad = 0;
    for (const auto& button : kButtons) {
        if (wpad & button.wpad)
            vpad |= button.vpad;
    }
    return vpad;
}

} // namespace Input
} // namespace Mss

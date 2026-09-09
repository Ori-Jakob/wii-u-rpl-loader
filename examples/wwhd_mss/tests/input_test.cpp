#include "../input.h"

#include <padscore/wpad.h>
#include <vpad/input.h>

#include <cstring>
#include <iostream>

namespace {

int gFailures = 0;

#define CHECK(condition)                                                        \
    do {                                                                        \
        if (!(condition)) {                                                     \
            std::cerr << __FILE__ << ':' << __LINE__                           \
                      << ": check failed: " #condition << '\n';                \
            ++gFailures;                                                        \
        }                                                                       \
    } while (false)

// What the host would report: one GamePad and four KPAD channels.
RplPad gPad;
bool gHavePad;
RplKpad gKpad[RPL_KPAD_CHANNELS];
bool gHaveKpad[RPL_KPAD_CHANNELS];

int HostPad(const RplHost*, RplPad* out)
{
    if (!gHavePad)
        return 0;
    *out = gPad;
    return 1;
}

int HostKpad(const RplHost*, uint32_t chan, RplKpad* out)
{
    if (chan >= RPL_KPAD_CHANNELS || !gHaveKpad[chan])
        return 0;
    *out = gKpad[chan];
    return 1;
}

RplHost gHost;

void ClearControllers()
{
    std::memset(&gPad, 0, sizeof(gPad));
    std::memset(gKpad, 0, sizeof(gKpad));
    std::memset(gHaveKpad, 0, sizeof(gHaveKpad));
    gHavePad = false;
    Mss::Input::Reset();
}

void GamePadHold(uint32_t buttons)
{
    gHavePad = true;
    gPad.hold = buttons;
}

void ProHold(uint32_t chan, uint32_t buttons)
{
    gHaveKpad[chan] = true;
    gKpad[chan].extension = WPAD_EXT_PRO_CONTROLLER;
    gKpad[chan].hold = buttons;
}

void CheckMacroChordFromGamePad()
{
    ClearControllers();
    GamePadHold(VPAD_BUTTON_A | VPAD_BUTTON_ZR);
    Mss::Input::Poll();
    CHECK(Mss::Input::Held(VPAD_BUTTON_A | VPAD_BUTTON_ZR));
    CHECK(!Mss::Input::Held(VPAD_BUTTON_A | VPAD_BUTTON_ZL));

    GamePadHold(VPAD_BUTTON_A);
    Mss::Input::Poll();
    CHECK(!Mss::Input::Held(VPAD_BUTTON_A | VPAD_BUTTON_ZR));
}

void CheckMacroChordFromPro()
{
    ClearControllers();
    ProHold(0, WPAD_PRO_BUTTON_A | WPAD_PRO_BUTTON_ZR);
    Mss::Input::Poll();
    CHECK(Mss::Input::Held(VPAD_BUTTON_A | VPAD_BUTTON_ZR));

    // A GamePad that reports nothing must not mask the Pro Controller.
    gHavePad = true;
    gPad.hold = 0;
    Mss::Input::Poll();
    CHECK(Mss::Input::Held(VPAD_BUTTON_A | VPAD_BUTTON_ZR));
}

void CheckChordSplitAcrossControllers()
{
    ClearControllers();
    GamePadHold(VPAD_BUTTON_A);
    ProHold(1, WPAD_PRO_BUTTON_ZR);
    Mss::Input::Poll();
    CHECK(Mss::Input::Held(VPAD_BUTTON_A | VPAD_BUTTON_ZR));
}

void CheckMenuChordEdges()
{
    constexpr uint32_t chord = VPAD_BUTTON_ZL | VPAD_BUTTON_L | VPAD_BUTTON_MINUS;
    ClearControllers();

    ProHold(0, WPAD_PRO_BUTTON_ZL | WPAD_PRO_BUTTON_L);
    Mss::Input::Poll();
    CHECK(!Mss::Input::Pressed(chord));

    ProHold(0, WPAD_PRO_BUTTON_ZL | WPAD_PRO_BUTTON_L | WPAD_PRO_BUTTON_MINUS);
    Mss::Input::Poll();
    CHECK(Mss::Input::Pressed(chord));  // the last key landed this poll

    Mss::Input::Poll();
    CHECK(!Mss::Input::Pressed(chord));  // still held, no new edge
    CHECK(Mss::Input::Held(chord));

    ProHold(0, WPAD_PRO_BUTTON_ZL | WPAD_PRO_BUTTON_L);
    Mss::Input::Poll();
    CHECK(Mss::Input::Released(chord));
    CHECK(!Mss::Input::Held(chord));
}

void CheckMappingRoundTrip()
{
    const uint32_t vpad = VPAD_BUTTON_A | VPAD_BUTTON_PLUS | VPAD_BUTTON_STICK_L |
                          VPAD_BUTTON_DOWN;
    CHECK(Mss::Input::VpadFromWpad(Mss::Input::WpadFromVpad(vpad)) == vpad);
    CHECK(Mss::Input::WpadFromVpad(VPAD_BUTTON_PLUS) == WPAD_PRO_BUTTON_PLUS);
    CHECK(Mss::Input::WpadFromVpad(VPAD_BUTTON_ZR) == WPAD_PRO_BUTTON_ZR);
    CHECK(Mss::Input::WpadFromVpad(VPAD_BUTTON_ZR) == WPAD_CLASSIC_BUTTON_ZR);
    CHECK(Mss::Input::VpadFromWpad(WPAD_PRO_BUTTON_A) == VPAD_BUTTON_A);
}

void CheckSticksFollowTheMovedController()
{
    ClearControllers();
    GamePadHold(0);
    ProHold(0, 0);
    gKpad[0].leftY = -1.0f;
    Mss::Input::Poll();
    CHECK(Mss::Input::Pad().leftY == -1.0f);

    gPad.leftY = 1.0f;
    gKpad[0].leftY = 0.2f;
    Mss::Input::Poll();
    CHECK(Mss::Input::Pad().leftY == 1.0f);
}

} // namespace

int main()
{
    std::memset(&gHost, 0, sizeof(gHost));
    gHost.pad = HostPad;
    gHost.kpad = HostKpad;
    Mss::Input::Bind(&gHost);

    CheckMacroChordFromGamePad();
    CheckMacroChordFromPro();
    CheckChordSplitAcrossControllers();
    CheckMenuChordEdges();
    CheckMappingRoundTrip();
    CheckSticksFollowTheMovedController();

    if (gFailures != 0) {
        std::cerr << gFailures << " check(s) failed\n";
        return 1;
    }
    std::cout << "input tests passed\n";
    return 0;
}

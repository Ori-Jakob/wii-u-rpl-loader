#include "rplloader/rpl_input.h"
#include "rplloader/rpl_loader.h"

#include <atomic>
#include <string.h>

#include <padscore/kpad.h>
#include <padscore/wpad.h>
#include <vpad/input.h>
#include <wups.h>

namespace Rpl {
namespace Input {

// Written on the input thread, read from a hook thread; the counter brackets the copy
static RplPad            s_pad;
static volatile uint32_t s_sample = 0;

static RplKpad           s_kpad[RPL_KPAD_CHANNELS];
static volatile uint32_t s_kpadSample[RPL_KPAD_CHANNELS];

static volatile int  s_mode = RPL_INPUT_PASS;
static volatile bool s_stickHeld = false;
static float         s_stickX = 0.0f, s_stickY = 0.0f;


static std::atomic<uint32_t> s_buttonsRequested{0};
static std::atomic<uint32_t> s_vpadButtonsApplied{0};
static std::atomic<uint32_t> s_kpadButtonsApplied[RPL_KPAD_CHANNELS]{};

static void publish(const VPADStatus& st)
{
    RplPad p;
    p.hold    = st.hold;
    p.trigger = st.trigger;
    p.release = st.release;
    p.leftX   = st.leftStick.x;
    p.leftY   = st.leftStick.y;
    p.rightX  = st.rightStick.x;
    p.rightY  = st.rightStick.y;
    p.touch.x        = st.tpNormal.x;
    p.touch.y        = st.tpNormal.y;
    p.touch.touched  = st.tpNormal.touched;
    p.touch.validity = st.tpNormal.validity;
    p.sample  = s_sample + 1;
    s_pad = p;
    s_sample = p.sample;
}

static void publishKpad(uint32_t chan, const KPADStatus& st)
{
    RplKpad k;
    memset(&k, 0, sizeof(k));
    k.extension = (uint32_t)st.extensionType;

    if (st.extensionType == WPAD_EXT_PRO_CONTROLLER) {
        k.hold    = st.pro.hold;
        k.trigger = st.pro.trigger;
        k.release = st.pro.release;
        k.leftX   = st.pro.leftStick.x;
        k.leftY   = st.pro.leftStick.y;
        k.rightX  = st.pro.rightStick.x;
        k.rightY  = st.pro.rightStick.y;
    } else if (st.extensionType == WPAD_EXT_CLASSIC ||
               st.extensionType == WPAD_EXT_MPLUS_CLASSIC) {
        k.hold    = st.classic.hold;
        k.trigger = st.classic.trigger;
        k.release = st.classic.release;
        k.leftX   = st.classic.leftStick.x;
        k.leftY   = st.classic.leftStick.y;
        k.rightX  = st.classic.rightStick.x;
        k.rightY  = st.classic.rightStick.y;
    }

    k.sample = s_kpadSample[chan] + 1;
    s_kpad[chan] = k;
    s_kpadSample[chan] = k.sample;
}

bool Get(RplPad* out)
{
    if (!out)
        return false;
    for (int attempt = 0; attempt < 4; ++attempt) {
        const uint32_t before = s_sample;
        if (before == 0)
            return false;
        RplPad copy = s_pad;
        if (s_sample == before && copy.sample == before) {
            *out = copy;
            return true;
        }
    }
    return false;
}

bool GetKpad(uint32_t chan, RplKpad* out)
{
    if (!out || chan >= RPL_KPAD_CHANNELS)
        return false;
    for (int attempt = 0; attempt < 4; ++attempt) {
        const uint32_t before = s_kpadSample[chan];
        if (before == 0)
            return false;
        RplKpad copy = s_kpad[chan];
        if (s_kpadSample[chan] == before && copy.sample == before) {
            *out = copy;
            return true;
        }
    }
    return false;
}

void SetMode(int mode)
{
    s_mode = mode == RPL_INPUT_BLOCK ? RPL_INPUT_BLOCK : RPL_INPUT_PASS;
}

void SetStick(const float* leftXY)
{
    if (!leftXY) {
        s_stickHeld = false;
        return;
    }
    s_stickX = leftXY[0];
    s_stickY = leftXY[1];
    s_stickHeld = true;
}

void SetButtons(uint32_t vpadButtonMask)
{
    s_buttonsRequested.store(vpadButtonMask, std::memory_order_release);
}

static uint32_t mapProButtons(uint32_t vpad)
{
    uint32_t pro = 0;
    if (vpad & VPAD_BUTTON_A)       pro |= WPAD_PRO_BUTTON_A;
    if (vpad & VPAD_BUTTON_B)       pro |= WPAD_PRO_BUTTON_B;
    if (vpad & VPAD_BUTTON_X)       pro |= WPAD_PRO_BUTTON_X;
    if (vpad & VPAD_BUTTON_Y)       pro |= WPAD_PRO_BUTTON_Y;
    if (vpad & VPAD_BUTTON_LEFT)    pro |= WPAD_PRO_BUTTON_LEFT;
    if (vpad & VPAD_BUTTON_RIGHT)   pro |= WPAD_PRO_BUTTON_RIGHT;
    if (vpad & VPAD_BUTTON_UP)      pro |= WPAD_PRO_BUTTON_UP;
    if (vpad & VPAD_BUTTON_DOWN)    pro |= WPAD_PRO_BUTTON_DOWN;
    if (vpad & VPAD_BUTTON_ZL)      pro |= WPAD_PRO_BUTTON_ZL;
    if (vpad & VPAD_BUTTON_ZR)      pro |= WPAD_PRO_BUTTON_ZR;
    if (vpad & VPAD_BUTTON_L)       pro |= WPAD_PRO_BUTTON_L;
    if (vpad & VPAD_BUTTON_R)       pro |= WPAD_PRO_BUTTON_R;
    if (vpad & VPAD_BUTTON_PLUS)    pro |= WPAD_PRO_BUTTON_PLUS;
    if (vpad & VPAD_BUTTON_MINUS)   pro |= WPAD_PRO_BUTTON_MINUS;
    if (vpad & VPAD_BUTTON_HOME)    pro |= WPAD_PRO_BUTTON_HOME;
    if (vpad & VPAD_BUTTON_STICK_L) pro |= WPAD_PRO_BUTTON_STICK_L;
    if (vpad & VPAD_BUTTON_STICK_R) pro |= WPAD_PRO_BUTTON_STICK_R;

    if (vpad & VPAD_STICK_L_EMULATION_LEFT)  pro |= WPAD_PRO_STICK_L_EMULATION_LEFT;
    if (vpad & VPAD_STICK_L_EMULATION_RIGHT) pro |= WPAD_PRO_STICK_L_EMULATION_RIGHT;
    if (vpad & VPAD_STICK_L_EMULATION_UP)    pro |= WPAD_PRO_STICK_L_EMULATION_UP;
    if (vpad & VPAD_STICK_L_EMULATION_DOWN)  pro |= WPAD_PRO_STICK_L_EMULATION_DOWN;
    if (vpad & VPAD_STICK_R_EMULATION_LEFT)  pro |= WPAD_PRO_STICK_R_EMULATION_LEFT;
    if (vpad & VPAD_STICK_R_EMULATION_RIGHT) pro |= WPAD_PRO_STICK_R_EMULATION_RIGHT;
    if (vpad & VPAD_STICK_R_EMULATION_UP)    pro |= WPAD_PRO_STICK_R_EMULATION_UP;
    if (vpad & VPAD_STICK_R_EMULATION_DOWN)  pro |= WPAD_PRO_STICK_R_EMULATION_DOWN;
    return pro;
}

static uint32_t mapClassicButtons(uint32_t vpad)
{
    uint32_t classic = 0;
    if (vpad & VPAD_BUTTON_A)       classic |= WPAD_CLASSIC_BUTTON_A;
    if (vpad & VPAD_BUTTON_B)       classic |= WPAD_CLASSIC_BUTTON_B;
    if (vpad & VPAD_BUTTON_X)       classic |= WPAD_CLASSIC_BUTTON_X;
    if (vpad & VPAD_BUTTON_Y)       classic |= WPAD_CLASSIC_BUTTON_Y;
    if (vpad & VPAD_BUTTON_LEFT)    classic |= WPAD_CLASSIC_BUTTON_LEFT;
    if (vpad & VPAD_BUTTON_RIGHT)   classic |= WPAD_CLASSIC_BUTTON_RIGHT;
    if (vpad & VPAD_BUTTON_UP)      classic |= WPAD_CLASSIC_BUTTON_UP;
    if (vpad & VPAD_BUTTON_DOWN)    classic |= WPAD_CLASSIC_BUTTON_DOWN;
    if (vpad & VPAD_BUTTON_ZL)      classic |= WPAD_CLASSIC_BUTTON_ZL;
    if (vpad & VPAD_BUTTON_ZR)      classic |= WPAD_CLASSIC_BUTTON_ZR;
    if (vpad & VPAD_BUTTON_L)       classic |= WPAD_CLASSIC_BUTTON_L;
    if (vpad & VPAD_BUTTON_R)       classic |= WPAD_CLASSIC_BUTTON_R;
    if (vpad & VPAD_BUTTON_PLUS)    classic |= WPAD_CLASSIC_BUTTON_PLUS;
    if (vpad & VPAD_BUTTON_MINUS)   classic |= WPAD_CLASSIC_BUTTON_MINUS;
    if (vpad & VPAD_BUTTON_HOME)    classic |= WPAD_CLASSIC_BUTTON_HOME;

    if (vpad & VPAD_STICK_L_EMULATION_LEFT)  classic |= WPAD_CLASSIC_STICK_L_EMULATION_LEFT;
    if (vpad & VPAD_STICK_L_EMULATION_RIGHT) classic |= WPAD_CLASSIC_STICK_L_EMULATION_RIGHT;
    if (vpad & VPAD_STICK_L_EMULATION_UP)    classic |= WPAD_CLASSIC_STICK_L_EMULATION_UP;
    if (vpad & VPAD_STICK_L_EMULATION_DOWN)  classic |= WPAD_CLASSIC_STICK_L_EMULATION_DOWN;
    if (vpad & VPAD_STICK_R_EMULATION_LEFT)  classic |= WPAD_CLASSIC_STICK_R_EMULATION_LEFT;
    if (vpad & VPAD_STICK_R_EMULATION_RIGHT) classic |= WPAD_CLASSIC_STICK_R_EMULATION_RIGHT;
    if (vpad & VPAD_STICK_R_EMULATION_UP)    classic |= WPAD_CLASSIC_STICK_R_EMULATION_UP;
    if (vpad & VPAD_STICK_R_EMULATION_DOWN)  classic |= WPAD_CLASSIC_STICK_R_EMULATION_DOWN;
    return classic;
}

// Merge a virtual hold into a native sample. Native edges that would be
// impossible while the virtual button is down are removed, then changes in
// the virtual mask produce one edge for this input source.
static void editButtons(uint32_t& hold, uint32_t& trigger, uint32_t& release,
                        uint32_t current, uint32_t previous)
{
    const uint32_t physicalHold = hold;
    const uint32_t physicalRelease = release;

    trigger &= ~previous;
    release &= ~current;
    trigger |= (current & ~previous) & ~(physicalHold | physicalRelease);
    release |= (previous & ~current) & ~physicalHold;
    hold = physicalHold | current;
}

// The sample is published before this runs, so an RPL still sees the buttons
// it is holding the controller with.
static void editVpad(VPADStatus* buffers, uint32_t count)
{
    const bool block = s_mode == RPL_INPUT_BLOCK;
    const bool stick = s_stickHeld;
    const uint32_t buttons = s_buttonsRequested.load(std::memory_order_acquire);
    const uint32_t previous = s_vpadButtonsApplied.exchange(buttons, std::memory_order_acq_rel);
    if (!block && !stick && buttons == 0 && previous == 0)
        return;

    for (uint32_t i = 0; i < count; ++i) {
        VPADStatus& s = buffers[i];
        if (block) {
            s.hold = s.trigger = s.release = 0;
            s.rightStick.x = s.rightStick.y = 0.0f;
            s.tpNormal.touched = 0;
            s.tpFiltered1.touched = 0;
            s.tpFiltered2.touched = 0;
        }
        if (stick) {
            s.leftStick.x = s_stickX;
            s.leftStick.y = s_stickY;
        } else if (block) {
            s.leftStick.x = s.leftStick.y = 0.0f;
        }
        editButtons(s.hold, s.trigger, s.release, buttons, i == 0 ? previous : buttons);
    }
}

static void editKpad(uint32_t chan, KPADStatus* buffers, uint32_t count)
{
    const bool block = s_mode == RPL_INPUT_BLOCK;
    const bool stick = s_stickHeld;
    const uint32_t buttons = s_buttonsRequested.load(std::memory_order_acquire);
    if (!block && !stick && buttons == 0 &&
        s_kpadButtonsApplied[chan].load(std::memory_order_relaxed) == 0)
        return;

    bool buttonsAdvanced = false;
    for (uint32_t i = 0; i < count; ++i) {
        KPADStatus& s = buffers[i];
        if (block) {
            s.hold = s.trigger = s.release = 0;
            s.pro.hold = s.pro.trigger = s.pro.release = 0;
            s.pro.rightStick.x = s.pro.rightStick.y = 0.0f;
            s.classic.hold = s.classic.trigger = s.classic.release = 0;
            s.classic.rightStick.x = s.classic.rightStick.y = 0.0f;
            s.nunchuk.hold = s.nunchuk.trigger = s.nunchuk.release = 0;
        }
        if (stick) {
            s.pro.leftStick.x = s_stickX;
            s.pro.leftStick.y = s_stickY;
            s.classic.leftStick.x = s_stickX;
            s.classic.leftStick.y = s_stickY;
        } else if (block) {
            s.pro.leftStick.x = s.pro.leftStick.y = 0.0f;
            s.classic.leftStick.x = s.classic.leftStick.y = 0.0f;
            s.nunchuk.stick.x = s.nunchuk.stick.y = 0.0f;
        }

        const bool pro = s.extensionType == WPAD_EXT_PRO_CONTROLLER;
        const bool classic = s.extensionType == WPAD_EXT_CLASSIC ||
                             s.extensionType == WPAD_EXT_MPLUS_CLASSIC;
        if (!pro && !classic)
            continue;

        const uint32_t previous = buttonsAdvanced
                                ? buttons
                                : s_kpadButtonsApplied[chan].exchange(buttons,
                                                                     std::memory_order_acq_rel);
        buttonsAdvanced = true;
        if (pro) {
            editButtons(s.pro.hold, s.pro.trigger, s.pro.release,
                        mapProButtons(buttons), mapProButtons(previous));
        } else {
            editButtons(s.classic.hold, s.classic.trigger, s.classic.release,
                        mapClassicButtons(buttons), mapClassicButtons(previous));
        }
    }
}

void Reset()
{
    memset(&s_pad, 0, sizeof(s_pad));
    s_sample = 0;
    memset(s_kpad, 0, sizeof(s_kpad));
    for (uint32_t i = 0; i < RPL_KPAD_CHANNELS; ++i)
        s_kpadSample[i] = 0;
    s_mode = RPL_INPUT_PASS;
    s_stickHeld = false;
    s_buttonsRequested.store(0, std::memory_order_release);
    s_vpadButtonsApplied.store(0, std::memory_order_relaxed);
    for (uint32_t i = 0; i < RPL_KPAD_CHANNELS; ++i)
        s_kpadButtonsApplied[i].store(0, std::memory_order_relaxed);
}

} // namespace Input
} // namespace Rpl

// Only channel 0's newest sample is kept; the title reads what SetMode allows
DECL_FUNCTION(int32_t, VPADRead, VPADChan chan, VPADStatus* buffers, uint32_t count, VPADReadError* error)
{
    const int32_t result = real_VPADRead(chan, buffers, count, error);
    if (chan != VPAD_CHAN_0 || !buffers || count == 0 || result <= 0 ||
        (error && *error != VPAD_READ_SUCCESS))
        return result;

    Rpl::Input::publish(buffers[0]);
    // Between the two on purpose: a plugin decides, from the sample that just
    // arrived, whether the title should see it at all.
    Rpl::Loader::OnPadSampled();
    Rpl::Input::editVpad(buffers, (uint32_t)result < count ? (uint32_t)result : count);
    return result;
}

DECL_FUNCTION(uint32_t, KPADReadEx, KPADChan chan, KPADStatus* buffers, uint32_t count, KPADError* error)
{
    const uint32_t result = real_KPADReadEx(chan, buffers, count, error);
    if (!buffers || count == 0 || result == 0 ||
        (uint32_t)chan >= RPL_KPAD_CHANNELS || (error && *error != KPAD_ERROR_OK))
        return result;

    Rpl::Input::publishKpad((uint32_t)chan, buffers[0]);
    Rpl::Input::editKpad((uint32_t)chan, buffers, result < count ? result : count);
    return result;
}

WUPS_MUST_REPLACE(VPADRead, WUPS_LOADER_LIBRARY_VPAD, VPADRead);
WUPS_MUST_REPLACE(KPADReadEx, WUPS_LOADER_LIBRARY_PADSCORE, KPADReadEx);

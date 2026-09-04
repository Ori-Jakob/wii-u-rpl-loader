#include "rplloader/rpl_input.h"

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

// The sample is published before this runs, so an RPL still sees the buttons
// it is holding the controller with.
static void editVpad(VPADStatus* buffers, uint32_t count)
{
    const bool block = s_mode == RPL_INPUT_BLOCK;
    const bool stick = s_stickHeld;
    if (!block && !stick)
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
    }
}

static void editKpad(KPADStatus* buffers, uint32_t count)
{
    const bool block = s_mode == RPL_INPUT_BLOCK;
    const bool stick = s_stickHeld;
    if (!block && !stick)
        return;

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
    Rpl::Input::editKpad(buffers, result < count ? result : count);
    return result;
}

WUPS_MUST_REPLACE(VPADRead, WUPS_LOADER_LIBRARY_VPAD, VPADRead);
WUPS_MUST_REPLACE(KPADReadEx, WUPS_LOADER_LIBRARY_PADSCORE, KPADReadEx);

#include "rplloader/rpl_input.h"

#include <string.h>

#include <vpad/input.h>
#include <wups.h>

namespace Rpl {
namespace Input {

// Written on the input thread, read from a hook thread; the counter brackets the copy
static RplPad           s_pad;
static volatile uint32_t s_sample = 0;

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
    p.sample  = s_sample + 1;
    s_pad = p;
    s_sample = p.sample;
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

void Reset()
{
    memset(&s_pad, 0, sizeof(s_pad));
    s_sample = 0;
}

} // namespace Input
} // namespace Rpl

// Only channel 0's newest sample is kept, everything passes through untouched
DECL_FUNCTION(int32_t, VPADRead, VPADChan chan, VPADStatus* buffers, uint32_t count, VPADReadError* error)
{
    const int32_t result = real_VPADRead(chan, buffers, count, error);
    if (chan == VPAD_CHAN_0 && buffers && count > 0 && result > 0 &&
        (!error || *error == VPAD_READ_SUCCESS))
        Rpl::Input::publish(buffers[0]);
    return result;
}

WUPS_MUST_REPLACE(VPADRead, WUPS_LOADER_LIBRARY_VPAD, VPADRead);

#include <vpad/input.h>

#include "libwwhd/libwwhd.h"
#include "wwhd_cheats.h"

namespace Cheats {
namespace {

uint32_t sFrames;
int      sSailApplied;

// The boat reloads this constant every frame so scaling speedF does nothing
void ApplySail(int multiplier)
{
    if (multiplier == sSailApplied)
        return;

    f32* speed = daShip_getSailSpeedPtr();
    if (!speed)
        return;

    *speed = multiplier ? WWHD_SAIL_SPEED_STOCK * f32(multiplier) : WWHD_SAIL_SPEED_STOCK;
    sSailApplied = multiplier;
}

void PerFrame()
{
    RplPad pad;
    // Reading the pad ourselves would steal samples the game expects
    if (!gHost || !gHost->pad(gHost, &pad))
        return;

    daPy_lk_c* link = daPy_lk_c_getPlayer();
    if (!link) {
        ApplySail(0);
        return;
    }

    // Set before the movement code integrates it
    if (pad.hold & VPAD_BUTTON_STICK_L)
        link->base.speed.y = gConfig.jump;

    const bool boosting = (pad.hold & VPAD_BUTTON_ZR) && daPy_isRidingShip();
    ApplySail(boosting ? gConfig.sailBoost : 0);
}

} // namespace

// fapGm_Execute calls this once per game frame and nothing else does
RPL_DECL_REPLACE(void, cCt_Counter, int reset)
{
    real_cCt_Counter(reset);

    if (++sFrames == 1)
        Log(RPL_LOG_INFO, "first game frame through the hook");
    PerFrame();
}

const RplHook gHooks[] = {
    RPL_REPLACE(cCt_Counter, 0x0200E6ECu, 0x3D401020u, RPL_HOOK_REQUIRED),
};
const uint32_t gHookCount = sizeof(gHooks) / sizeof(gHooks[0]);

void ResetHooks()
{
    ApplySail(0);
}

} // namespace Cheats

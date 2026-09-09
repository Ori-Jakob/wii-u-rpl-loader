#include "../macro_engine.h"

#include <cmath>
#include <iostream>
#include <limits>

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

bool Near(float lhs, float rhs)
{
    return std::fabs(lhs - rhs) < 0.0001f;
}

Mss::MacroEngine EnabledEngine(uint32_t delayMs = 100u)
{
    Mss::MacroSettings settings;
    settings.enabled = true;
    settings.plusDelayMs = delayMs;
    return Mss::MacroEngine(settings);
}

void CheckDefaultsAndClamps()
{
    Mss::MacroEngine engine;
    CHECK(!engine.GetSettings().enabled);
    CHECK(engine.GetSettings().plusDelayMs == 100u);
    CHECK(engine.GetSettings().stickEnabled);
    CHECK(Near(engine.GetSettings().stickStrength, 1.0f));

    const Mss::MacroOutput disabled = engine.Step(true, 0);
    CHECK(!disabled.plusHeld);
    CHECK(!disabled.stickOverride);

    Mss::MacroSettings settings;
    settings.enabled = true;
    settings.plusDelayMs = 50000u;
    settings.stickStrength = 2.0f;
    engine.SetSettings(settings);
    CHECK(engine.GetSettings().plusDelayMs == Mss::kMaximumPlusDelayMs);
    CHECK(Near(engine.GetSettings().stickStrength, 1.0f));

    engine.SetStickStrength(-0.25f);
    CHECK(Near(engine.GetSettings().stickStrength, 0.0f));
    engine.SetStickStrength(std::numeric_limits<float>::quiet_NaN());
    CHECK(Near(engine.GetSettings().stickStrength, 0.0f));
}

// 30 fps: one step every 33 ms.
void CheckExactSequence()
{
    Mss::MacroEngine engine = EnabledEngine();
    uint64_t t = 1000;

    Mss::MacroOutput output = engine.Step(true, t);
    CHECK(output.phase == Mss::MacroPhase::Open);
    CHECK(output.plusHeld);
    CHECK(output.stickOverride);
    CHECK(Near(output.stickX, 0.0f));
    CHECK(Near(output.stickY, -1.0f));

    // Paused from the first released frame; the delay counts from there.
    for (int frame = 0; frame < 4; ++frame) {
        t += 33;
        output = engine.Step(true, t);
        CHECK(output.phase == Mss::MacroPhase::Paused);
        CHECK(!output.plusHeld);
        CHECK(Near(output.stickY, -1.0f));  // the stick does not move while paused
    }

    // 132 ms into the pause: close, and the flip rides on this same press.
    t += 33;
    output = engine.Step(true, t);
    CHECK(output.phase == Mss::MacroPhase::Close);
    CHECK(output.plusHeld);
    CHECK(Near(output.stickY, 1.0f));

    for (int frame = 0; frame < 6; ++frame) {
        t += 33;
        output = engine.Step(true, t);
        CHECK(output.phase == Mss::MacroPhase::Swim);
        CHECK(!output.plusHeld);
        CHECK(Near(output.stickY, 1.0f));
    }

    // The host sees the reversal: open on this very step, stick unchanged.
    t += 33;
    output = engine.Step(true, t, true);
    CHECK(output.phase == Mss::MacroPhase::Open);
    CHECK(output.plusHeld);
    CHECK(Near(output.stickY, 1.0f));

    for (int frame = 0; frame < 4; ++frame) {
        t += 33;
        output = engine.Step(true, t);
        CHECK(output.phase == Mss::MacroPhase::Paused);
        CHECK(Near(output.stickY, 1.0f));
    }
    t += 33;
    output = engine.Step(true, t);
    CHECK(output.phase == Mss::MacroPhase::Close);
    CHECK(output.plusHeld);
    CHECK(Near(output.stickY, -1.0f));
}

void CheckSwimCapWithoutReversal()
{
    Mss::MacroEngine engine = EnabledEngine(0);
    CHECK(engine.Step(true, 0).phase == Mss::MacroPhase::Open);
    CHECK(engine.Step(true, 1).phase == Mss::MacroPhase::Paused);
    CHECK(engine.Step(true, 2).phase == Mss::MacroPhase::Close);
    for (uint32_t frame = 0; frame < Mss::kSwimFrames; ++frame)
        CHECK(engine.Step(true, 3 + frame).phase == Mss::MacroPhase::Swim);
    CHECK(engine.Step(true, 20).phase == Mss::MacroPhase::Open);

    // A reversal reported outside the swim phase changes nothing.
    CHECK(engine.Step(true, 21, true).phase == Mss::MacroPhase::Paused);
}

void CheckZeroDelayStillReleasesPlus()
{
    Mss::MacroEngine engine = EnabledEngine(0);

    CHECK(engine.Step(true, 5).plusHeld);

    const Mss::MacroOutput release = engine.Step(true, 5);
    CHECK(release.phase == Mss::MacroPhase::Paused);
    CHECK(!release.plusHeld);

    const Mss::MacroOutput second = engine.Step(true, 5);
    CHECK(second.phase == Mss::MacroPhase::Close);
    CHECK(second.plusHeld);
}

void CheckResetAndActivation()
{
    Mss::MacroEngine engine = EnabledEngine();
    CHECK(engine.Step(true, 100).plusHeld);
    CHECK(!engine.Step(true, 150).plusHeld);

    const Mss::MacroOutput inactive = engine.Step(false, 151);
    CHECK(!inactive.plusHeld);
    CHECK(!inactive.stickOverride);

    const Mss::MacroOutput restarted = engine.Step(true, 1000);
    CHECK(restarted.phase == Mss::MacroPhase::Open);
    CHECK(restarted.plusHeld);
    CHECK(Near(restarted.stickY, -1.0f));

    engine.SetEnabled(false);
    CHECK(!engine.Step(true, 1001).plusHeld);
    engine.SetEnabled(true);
    CHECK(engine.Step(true, 1002).plusHeld);
}

void CheckLiveSettings()
{
    Mss::MacroEngine engine = EnabledEngine();
    CHECK(engine.Step(true, 0).plusHeld);

    engine.SetPlusDelayMs(50);
    CHECK(engine.Step(true, 10).phase == Mss::MacroPhase::Paused);  // delay starts here
    CHECK(engine.Step(true, 59).phase == Mss::MacroPhase::Paused);
    CHECK(engine.Step(true, 60).phase == Mss::MacroPhase::Close);

    engine.SetStickEnabled(false);
    const Mss::MacroOutput noStick = engine.Step(true, 61);
    CHECK(noStick.phase == Mss::MacroPhase::Swim);
    CHECK(!noStick.plusHeld);
    CHECK(!noStick.stickOverride);
    CHECK(Near(noStick.stickX, 0.0f));
    CHECK(Near(noStick.stickY, 0.0f));

    engine.SetStickEnabled(true);
    engine.SetStickStrength(0.4f);
    const Mss::MacroOutput weakStick = engine.Step(true, 62);
    CHECK(weakStick.stickOverride);
    CHECK(Near(weakStick.stickY, 0.4f));  // flipped up at the close
}

void CheckPauseOnRelease()
{
    Mss::MacroEngine engine = EnabledEngine(0);
    CHECK(engine.GetSettings().pauseOnRelease);

    // Let go while paused: the game already is, stop at once.
    CHECK(engine.Step(true, 0).phase == Mss::MacroPhase::Open);
    CHECK(engine.Step(true, 1).phase == Mss::MacroPhase::Paused);
    Mss::MacroOutput output = engine.Step(false, 2);
    CHECK(!output.plusHeld);
    CHECK(!output.stickOverride);

    // Let go after a close: hold the flipped stick, open on the reversal, stop.
    CHECK(engine.Step(true, 3).phase == Mss::MacroPhase::Open);
    CHECK(engine.Step(true, 4).phase == Mss::MacroPhase::Paused);
    output = engine.Step(true, 5);
    CHECK(output.phase == Mss::MacroPhase::Close);
    CHECK(Near(output.stickY, 1.0f));
    output = engine.Step(false, 6);
    CHECK(output.phase == Mss::MacroPhase::Swim);
    CHECK(!output.plusHeld);
    CHECK(output.stickOverride);
    CHECK(Near(output.stickY, 1.0f));
    output = engine.Step(false, 7, true);
    CHECK(output.phase == Mss::MacroPhase::Open);
    CHECK(output.plusHeld);
    CHECK(Near(output.stickY, 1.0f));
    output = engine.Step(false, 8);
    CHECK(!output.plusHeld);
    CHECK(!output.stickOverride);

    // The cap still ends the wait when no reversal is ever reported.
    CHECK(engine.Step(true, 9).phase == Mss::MacroPhase::Open);
    CHECK(engine.Step(true, 10).phase == Mss::MacroPhase::Paused);
    CHECK(engine.Step(true, 11).phase == Mss::MacroPhase::Close);
    for (uint32_t frame = 0; frame < Mss::kSwimFrames; ++frame)
        CHECK(engine.Step(false, 12 + frame).phase == Mss::MacroPhase::Swim);
    output = engine.Step(false, 30);
    CHECK(output.phase == Mss::MacroPhase::Open);
    CHECK(output.plusHeld);
    CHECK(!engine.Step(false, 31).stickOverride);

    // Pressing again while finishing simply carries on with the cycle.
    CHECK(engine.Step(true, 40).phase == Mss::MacroPhase::Open);
    CHECK(engine.Step(true, 41).phase == Mss::MacroPhase::Paused);
    CHECK(engine.Step(true, 42).phase == Mss::MacroPhase::Close);
    CHECK(engine.Step(false, 43).phase == Mss::MacroPhase::Swim);
    CHECK(engine.Step(true, 44, true).phase == Mss::MacroPhase::Open);
    CHECK(engine.Step(true, 45).phase == Mss::MacroPhase::Paused);

    // Without the option, release is immediate whatever the phase.
    Mss::MacroSettings settings = engine.GetSettings();
    settings.pauseOnRelease = false;
    engine.SetSettings(settings);
    CHECK(engine.Step(true, 50).phase == Mss::MacroPhase::Close);
    output = engine.Step(false, 51);
    CHECK(!output.plusHeld);
    CHECK(!output.stickOverride);
}

void CheckClockRollback()
{
    Mss::MacroEngine engine = EnabledEngine();
    CHECK(engine.Step(true, 1000).plusHeld);
    CHECK(engine.Step(true, 1100).phase == Mss::MacroPhase::Paused);
    CHECK(engine.Step(true, 900).phase == Mss::MacroPhase::Paused);  // clock went back
    CHECK(engine.Step(true, 999).phase == Mss::MacroPhase::Paused);
    CHECK(engine.Step(true, 1000).phase == Mss::MacroPhase::Close);
}

} // namespace

int main()
{
    CheckDefaultsAndClamps();
    CheckExactSequence();
    CheckSwimCapWithoutReversal();
    CheckZeroDelayStillReleasesPlus();
    CheckResetAndActivation();
    CheckLiveSettings();
    CheckPauseOnRelease();
    CheckClockRollback();

    if (gFailures != 0) {
        std::cerr << gFailures << " macro engine test(s) failed\n";
        return 1;
    }

    std::cout << "All macro engine tests passed\n";
    return 0;
}

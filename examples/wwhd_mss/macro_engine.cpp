#include "macro_engine.h"

namespace Mss {
namespace {

float ClampStickStrength(float strength) noexcept
{
    if (!(strength >= 0.0f))
        return 0.0f;
    if (strength > 1.0f)
        return 1.0f;
    return strength;
}

} // namespace

MacroEngine::MacroEngine() noexcept
    : MacroEngine(MacroSettings{})
{
}

MacroEngine::MacroEngine(const MacroSettings& settings) noexcept
    : settings_(NormalizeSettings(settings)),
      phase_(MacroPhase::Open),
      delayStartMs_(0),
      pausedFrames_(0),
      swimFrames_(0),
      stickDirectionDown_(true),
      running_(false),
      finishing_(false)
{
}

void MacroEngine::Reset() noexcept
{
    phase_ = MacroPhase::Open;
    delayStartMs_ = 0;
    pausedFrames_ = 0;
    swimFrames_ = 0;
    stickDirectionDown_ = true;
    running_ = false;
    finishing_ = false;
}

void MacroEngine::SetSettings(const MacroSettings& settings) noexcept
{
    settings_ = NormalizeSettings(settings);
    if (!settings_.enabled)
        Reset();
}

const MacroSettings& MacroEngine::GetSettings() const noexcept
{
    return settings_;
}

void MacroEngine::SetEnabled(bool enabled) noexcept
{
    settings_.enabled = enabled;
    if (!enabled)
        Reset();
}

void MacroEngine::SetPlusDelayMs(uint32_t delayMs) noexcept
{
    settings_.plusDelayMs = delayMs > kMaximumPlusDelayMs
                                ? kMaximumPlusDelayMs
                                : delayMs;
}

void MacroEngine::SetStickEnabled(bool enabled) noexcept
{
    settings_.stickEnabled = enabled;
}

void MacroEngine::SetStickStrength(float strength) noexcept
{
    settings_.stickStrength = ClampStickStrength(strength);
}

MacroOutput MacroEngine::Step(bool active, uint64_t nowMs, bool reversalSeen) noexcept
{
    if (!settings_.enabled) {
        Reset();
        return MakeOutput(MacroPhase::Open, false, false);
    }

    if (active) {
        finishing_ = false;
    } else {
        const bool resuming = phase_ == MacroPhase::Swim ||
                              phase_ == MacroPhase::Open;
        if (!running_ || !settings_.pauseOnRelease || !resuming) {
            Reset();
            return MakeOutput(MacroPhase::Open, false, false);
        }
        finishing_ = true;
    }

    if (!running_) {
        running_ = true;
        phase_ = MacroPhase::Open;
        pausedFrames_ = 0;
        swimFrames_ = 0;
        stickDirectionDown_ = true;
    }

    if (phase_ == MacroPhase::Paused && pausedFrames_ > 0) {
        if (nowMs < delayStartMs_) {
            delayStartMs_ = nowMs;
        }
        if (nowMs - delayStartMs_ >= settings_.plusDelayMs)
            phase_ = MacroPhase::Close;
    }
    if (phase_ == MacroPhase::Swim && reversalSeen)
        phase_ = MacroPhase::Open;

    switch (phase_) {
    case MacroPhase::Open: {
        const MacroOutput output = MakeOutput(phase_, true, true);
        if (finishing_) {
            Reset();
            return output;
        }
        phase_ = MacroPhase::Paused;
        pausedFrames_ = 0;
        return output;
    }

    case MacroPhase::Paused:
        if (pausedFrames_++ == 0)
            delayStartMs_ = nowMs;
        return MakeOutput(phase_, false, true);

    case MacroPhase::Close: {
        stickDirectionDown_ = !stickDirectionDown_;
        const MacroOutput output = MakeOutput(phase_, true, true);
        phase_ = MacroPhase::Swim;
        swimFrames_ = 0;
        return output;
    }

    case MacroPhase::Swim: {
        const MacroOutput output = MakeOutput(phase_, false, true);
        if (++swimFrames_ >= kSwimFrames)
            phase_ = MacroPhase::Open;
        return output;
    }
    }

    Reset();
    return MakeOutput(MacroPhase::Open, false, false);
}

MacroSettings MacroEngine::NormalizeSettings(const MacroSettings& settings) noexcept
{
    MacroSettings normalized = settings;
    if (normalized.plusDelayMs > kMaximumPlusDelayMs)
        normalized.plusDelayMs = kMaximumPlusDelayMs;
    normalized.stickStrength = ClampStickStrength(normalized.stickStrength);
    return normalized;
}

MacroOutput MacroEngine::MakeOutput(MacroPhase phase, bool plusHeld,
                                    bool applyStick) const noexcept
{
    MacroOutput output;
    output.plusHeld = plusHeld;
    output.phase = phase;

    if (applyStick && settings_.stickEnabled) {
        output.stickOverride = true;
        output.stickY = stickDirectionDown_ ? -settings_.stickStrength
                                            : settings_.stickStrength;
    }

    return output;
}

} // namespace Mss

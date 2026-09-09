#pragma once

#include <stdint.h>

namespace Mss {

constexpr uint32_t kDefaultPlusDelayMs = 100u;
constexpr uint32_t kMaximumPlusDelayMs = 30000u;
constexpr float kDefaultStickStrength = 1.0f;

constexpr uint32_t kSwimFrames = 8u;

enum class MacroPhase : uint8_t {
    Open,
    Paused,
    Close,
    Swim,
};

struct MacroSettings {
    bool enabled = false;
    uint32_t plusDelayMs = kDefaultPlusDelayMs;
    bool stickEnabled = true;
    float stickStrength = kDefaultStickStrength;
    bool pauseOnRelease = true;
};

struct MacroOutput {
    bool plusHeld = false;
    bool stickOverride = false;
    float stickX = 0.0f;
    float stickY = 0.0f;
    MacroPhase phase = MacroPhase::Open;
};

class MacroEngine {
public:
    MacroEngine() noexcept;
    explicit MacroEngine(const MacroSettings& settings) noexcept;

    void Reset() noexcept;

    void SetSettings(const MacroSettings& settings) noexcept;
    const MacroSettings& GetSettings() const noexcept;

    void SetEnabled(bool enabled) noexcept;
    void SetPlusDelayMs(uint32_t delayMs) noexcept;
    void SetStickEnabled(bool enabled) noexcept;
    void SetStickStrength(float strength) noexcept;

    MacroOutput Step(bool active, uint64_t nowMs, bool reversalSeen = false) noexcept;

private:
    static MacroSettings NormalizeSettings(const MacroSettings& settings) noexcept;
    MacroOutput MakeOutput(MacroPhase phase, bool plusHeld,
                           bool applyStick) const noexcept;

    MacroSettings settings_;
    MacroPhase phase_;
    uint64_t delayStartMs_;
    uint32_t pausedFrames_;
    uint32_t swimFrames_;
    bool stickDirectionDown_;
    bool running_;
    bool finishing_;
};

} // namespace Mss

#pragma once

#include <stdint.h>

#include <rplloader/rplloader.h>

namespace Mss {
namespace Settings {

constexpr const char* kMacroEnabledKey = "macro_enabled";
constexpr const char* kPlusDelayKey = "plus_delay_ms";
constexpr const char* kStickEnabledKey = "stick_enabled";
constexpr const char* kPauseOnReleaseKey = "pause_on_release";
constexpr const char* kWatermarkOnlyComboKey = "watermark_only_combo";

struct Snapshot {
    bool macroEnabled;
    uint32_t plusDelayMs;
    bool stickEnabled;
    bool pauseOnRelease;
    bool watermarkOnlyCombo;
};

void Init(const RplHost* host);
Snapshot Get();

void SetMacroEnabled(bool enabled);
void PreviewPlusDelayMs(uint32_t delayMs);
void SetPlusDelayMs(uint32_t delayMs);
void SetStickEnabled(bool enabled);
void SetPauseOnRelease(bool enabled);
void SetWatermarkOnlyCombo(bool enabled);

uint32_t Crc32();
uint32_t Crc32Bytes(const void* data, uint32_t size);

} // namespace Settings
} // namespace Mss

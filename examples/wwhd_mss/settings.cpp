#include "settings.h"

#include "macro_engine.h"

#include <atomic>
#include <stdio.h>

namespace Mss {
namespace Settings {
namespace {

const RplHost* sHost = nullptr;

std::atomic<uint32_t> sMacroEnabled{1u};
std::atomic<uint32_t> sPlusDelayMs{kDefaultPlusDelayMs};
std::atomic<uint32_t> sStickEnabled{1u};
std::atomic<uint32_t> sPauseOnRelease{1u};
std::atomic<uint32_t> sWatermarkOnlyCombo{1u};

bool ReadBool(const char* key, bool fallback)
{
    return sHost && sHost->getBool
               ? sHost->getBool(sHost, key, fallback ? 1 : 0) != 0
               : fallback;
}

uint32_t ReadDelay()
{
    if (!sHost || !sHost->getInt)
        return kDefaultPlusDelayMs;

    int32_t value = sHost->getInt(sHost, kPlusDelayKey,
                                  int32_t(kDefaultPlusDelayMs));
    if (value < 0)
        value = 0;
    if (uint32_t(value) > kMaximumPlusDelayMs)
        value = int32_t(kMaximumPlusDelayMs);
    return uint32_t(value);
}

void StoreBool(const char* key, bool value)
{
    if (sHost && sHost->setBool)
        sHost->setBool(sHost, key, value ? 1 : 0);
}

} // namespace

void Init(const RplHost* host)
{
    sHost = host;
    sMacroEnabled.store(ReadBool(kMacroEnabledKey, true),
                        std::memory_order_relaxed);
    sPlusDelayMs.store(ReadDelay(), std::memory_order_relaxed);
    sStickEnabled.store(ReadBool(kStickEnabledKey, true),
                        std::memory_order_relaxed);
    sPauseOnRelease.store(ReadBool(kPauseOnReleaseKey, true),
                          std::memory_order_relaxed);
    sWatermarkOnlyCombo.store(ReadBool(kWatermarkOnlyComboKey, true),
                              std::memory_order_relaxed);
}

Snapshot Get()
{
    return {
        sMacroEnabled.load(std::memory_order_relaxed) != 0,
        sPlusDelayMs.load(std::memory_order_relaxed),
        sStickEnabled.load(std::memory_order_relaxed) != 0,
        sPauseOnRelease.load(std::memory_order_relaxed) != 0,
        sWatermarkOnlyCombo.load(std::memory_order_relaxed) != 0,
    };
}

void SetMacroEnabled(bool enabled)
{
    sMacroEnabled.store(enabled ? 1u : 0u, std::memory_order_relaxed);
    StoreBool(kMacroEnabledKey, enabled);
}

void PreviewPlusDelayMs(uint32_t delayMs)
{
    if (delayMs > kMaximumPlusDelayMs)
        delayMs = kMaximumPlusDelayMs;
    sPlusDelayMs.store(delayMs, std::memory_order_relaxed);
}

void SetPlusDelayMs(uint32_t delayMs)
{
    PreviewPlusDelayMs(delayMs);
    delayMs = sPlusDelayMs.load(std::memory_order_relaxed);
    if (sHost && sHost->setInt)
        sHost->setInt(sHost, kPlusDelayKey, int32_t(delayMs));
}

void SetStickEnabled(bool enabled)
{
    sStickEnabled.store(enabled ? 1u : 0u, std::memory_order_relaxed);
    StoreBool(kStickEnabledKey, enabled);
}

void SetPauseOnRelease(bool enabled)
{
    sPauseOnRelease.store(enabled ? 1u : 0u, std::memory_order_relaxed);
    StoreBool(kPauseOnReleaseKey, enabled);
}

void SetWatermarkOnlyCombo(bool enabled)
{
    sWatermarkOnlyCombo.store(enabled ? 1u : 0u, std::memory_order_relaxed);
    StoreBool(kWatermarkOnlyComboKey, enabled);
}

uint32_t Crc32Bytes(const void* data, uint32_t size)
{
    const uint8_t* bytes = static_cast<const uint8_t*>(data);
    uint32_t crc = 0xFFFFFFFFu;
    for (uint32_t i = 0; i < size; ++i) {
        crc ^= bytes[i];
        for (int bit = 0; bit < 8; ++bit)
            crc = (crc >> 1) ^ (0xEDB88320u & (0u - (crc & 1u)));
    }
    return ~crc;
}

uint32_t Crc32()
{
    char text[48];
    const int length = snprintf(text, sizeof(text), "plus_delay_ms=%u;stick_enabled=%u;",
                                unsigned(sPlusDelayMs.load(std::memory_order_relaxed)),
                                unsigned(sStickEnabled.load(std::memory_order_relaxed) != 0));
    return Crc32Bytes(text, uint32_t(length));
}

} // namespace Settings
} // namespace Mss

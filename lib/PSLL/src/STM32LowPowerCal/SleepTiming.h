#pragma once

#include <stdint.h>

namespace SleepTiming {
constexpr bool validFactor(float factor) {
    return factor >= 0.5f && factor <= 1.5f;
}

// Convert a wall-clock delay to RTC milliseconds, with an optional early wake.
constexpr uint32_t rtcDelay(uint32_t delayMs, uint32_t leadMs, float factor) {
    if (delayMs <= leadMs) return 0;
    const double ticks = double(delayMs - leadMs) *
                         (validFactor(factor) ? double(factor) : 1.0);
    return ticks >= 4294967295.0 ? UINT32_MAX : uint32_t(ticks + 0.5);
}

// Unsigned subtraction supports a wrapping 32-bit RTC millisecond timestamp.
constexpr uint32_t remaining(uint32_t start, uint32_t now, uint32_t delayMs,
                             uint32_t leadMs, float factor) {
    const uint32_t target = rtcDelay(delayMs, leadMs, factor);
    const uint32_t elapsed = now - start;
    return elapsed < target ? target - elapsed : 0;
}
} // namespace SleepTiming

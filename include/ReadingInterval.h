#pragma once

#include <stddef.h>
#include <stdint.h>

namespace ReadingInterval {
constexpr uint8_t PORT = 2;
constexpr uint8_t SET_INTERVAL = 1;
constexpr uint32_t DEFAULT_SECONDS = 600;
constexpr uint32_t MIN_SECONDS = 60;
constexpr uint32_t MAX_SECONDS = 86400;

// Command: 0x01 followed by a uint32_t number of seconds, most significant byte first.
// Invalid commands leave the current setting unchanged.
constexpr bool apply(uint8_t port, const uint8_t *data, size_t length,
                  uint32_t &intervalSeconds) {
    if (port != PORT || data == nullptr || length != 5 || data[0] != SET_INTERVAL) {
        return false;
    }
    const uint32_t seconds = (uint32_t(data[1]) << 24) |
                             (uint32_t(data[2]) << 16) |
                             (uint32_t(data[3]) << 8) | uint32_t(data[4]);
    if (seconds < MIN_SECONDS || seconds > MAX_SECONDS) {
        return false;
    }
    intervalSeconds = seconds;
    return true;
}
} // namespace ReadingInterval

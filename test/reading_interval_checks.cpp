// Compile with C++14 or newer. Static assertions execute the real parser during
// compilation, including state preservation, without an MCU or host runtime.
#include "ReadingInterval.h"

constexpr bool accepts(uint32_t seconds) {
    const uint8_t command[] = {1, uint8_t(seconds >> 24), uint8_t(seconds >> 16),
                              uint8_t(seconds >> 8), uint8_t(seconds)};
    uint32_t current = 1234;
    return ReadingInterval::apply(2, command, sizeof(command), current) && current == seconds;
}

constexpr bool rejects(uint32_t seconds, uint8_t port = 2, uint8_t opcode = 1, size_t length = 5) {
    const uint8_t command[] = {opcode, uint8_t(seconds >> 24), uint8_t(seconds >> 16),
                              uint8_t(seconds >> 8), uint8_t(seconds), 0};
    uint32_t current = 1234;
    return !ReadingInterval::apply(port, command, length, current) && current == 1234;
}

constexpr bool rejectsTruncated() {
    for (size_t length = 0; length < 5; ++length) {
        if (!rejects(600, 2, 1, length)) return false;
    }
    uint32_t current = 1234;
    return !ReadingInterval::apply(2, nullptr, 5, current) && current == 1234;
}

constexpr bool repeatedCommand() {
    const uint8_t command[] = {1, 0, 0, 0x07, 0x08};
    uint32_t current = 600;
    return ReadingInterval::apply(2, command, 5, current) && current == 1800 &&
           ReadingInterval::apply(2, command, 5, current) && current == 1800;
}

static_assert(accepts(60), "Minimum interval");
static_assert(accepts(600), "Default interval, with embedded zero bytes");
static_assert(accepts(1800), "30-minute documentation example");
static_assert(accepts(3600), "One-hour documentation example");
static_assert(accepts(86400), "24 hours requires more than 16 bits");
static_assert(rejects(0), "Zero must not disable timed wakeup");
static_assert(rejects(59), "Below lower bound");
static_assert(rejects(86401), "Above upper bound");
static_assert(rejects(0xFFFFFFFF), "Must not overflow milliseconds conversion");
static_assert(rejects(600, 0), "MAC port cannot change application settings");
static_assert(rejects(600, 1), "Uplink port cannot change application settings");
static_assert(rejects(600, 223), "Unrelated application port");
static_assert(rejects(600, 2, 0xFF), "Unknown command");
static_assert(rejects(600, 2, 1, 6), "Trailing bytes rejected");
static_assert(rejectsTruncated(), "Short and null payloads preserve current setting");
static_assert(repeatedCommand(), "Duplicate commands are idempotent");

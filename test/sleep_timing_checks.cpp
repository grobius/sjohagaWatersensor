#include "SleepTiming.h"

static_assert(SleepTiming::rtcDelay(600000, 0, 1.05f) == 630000,
              "Fast RTC needs more timer ticks for a ten-minute sleep");
static_assert(SleepTiming::rtcDelay(600000, 0, 0.95f) == 570000,
              "Slow RTC needs fewer timer ticks");
static_assert(SleepTiming::rtcDelay(5000, 20, 1.05f) == 5229,
              "RX1 early-wake margin is in wall-clock milliseconds");
static_assert(SleepTiming::remaining(10000, 10010, 5000, 20, 1.05f) == 5219,
              "Account for processing time after TX");
static_assert(SleepTiming::remaining(10000, 15300, 6000, 20, 1.05f) == 979,
              "RX2 deadline must include time spent in RX1");
static_assert(SleepTiming::remaining(10000, 16500, 6000, 20, 1.05f) == 0,
              "An expired deadline must not wrap into a long sleep");
static_assert(SleepTiming::remaining(0xFFFFFFF0U, 0x10U, 100, 20, 1.0f) == 48,
              "Timestamp rollover");
static_assert(SleepTiming::rtcDelay(20, 20, 1.0f) == 0 &&
              SleepTiming::rtcDelay(10, 20, 1.0f) == 0,
              "No negative sleep when the early-wake margin exceeds the delay");
static_assert(SleepTiming::rtcDelay(60000, 0, 0.0f) == 60000,
              "Invalid factors use an uncorrected delay");
static_assert(!SleepTiming::validFactor(0) && !SleepTiming::validFactor(-1) &&
              !SleepTiming::validFactor(2), "Reject invalid calibration");
static_assert(SleepTiming::rtcDelay(86400000, 0, 1.05f) >= 90719990 &&
              SleepTiming::rtcDelay(86400000, 0, 1.05f) <= 90720010,
              "A corrected 24-hour interval fits the timer, allowing float rounding");

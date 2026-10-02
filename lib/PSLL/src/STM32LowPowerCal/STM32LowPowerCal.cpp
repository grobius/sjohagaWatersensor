/*
@author Leo Korbee
@file   STM32LowPowerCal.cpp
@brief  Wrapper for LowPower - Correction for using LowPower in sleep modes with no crystal
time_correction factor is the factor that the millis time should be devided by to get the correct time.
and the correct time should be multiplied by to get millis internal to execute sleep
*/

#include "STM32LowPower.h"
#include "STM32LowPowerCal.h"
#include "SleepTiming.h"

// correction times for time used by powerdown and wakeup.
// substract for correction
void STM32LowPowerCal::deepSleep(uint32_t ms, int substraction_time)
{
    const uint32_t leadMs = substraction_time > 0 ? uint32_t(substraction_time) : 0;
    const uint32_t ticks = SleepTiming::rtcDelay(ms, leadMs, time_Correction_Factor);
    // deepSleep(0) means sleep indefinitely, not skip the wait.
    if (ticks != 0) LowPower.deepSleep(ticks);
}

float STM32LowPowerCal::getTimeCorrectionFactor()
{
    return time_Correction_Factor;
}

void STM32LowPowerCal::setTimeCorrectionFactor(float time_Correction_Factor)
{
    if (SleepTiming::validFactor(time_Correction_Factor)) {
        this->time_Correction_Factor = time_Correction_Factor;
    }
}

uint32_t STM32LowPowerCal::rtcMillis()
{
    uint32_t subSeconds;
    const uint32_t seconds = STM32RTC::getInstance().getEpoch(&subSeconds);
    return seconds * 1000UL + subSeconds;
}

bool STM32LowPowerCal::calibrateRTC(uint32_t sampleMs)
{
    if (sampleMs < 1000 || sampleMs > 60000) return false;
    STM32RTC &rtc = STM32RTC::getInstance();
    // Use the same RTC source and prescalers that deepSleep() uses.
    if (rtc.getClockSource() == STM32RTC::HSE_CLOCK) {
        rtc.setClockSource(STM32RTC::LSI_CLOCK);
    }
    rtc.begin();
    if (!rtc.isTimeSet()) rtc.setTime(12, 0, 0);
    const uint32_t startMs = millis();
    const uint32_t startRtc = rtcMillis();
    delay(sampleMs);
    const uint32_t elapsedRtc = rtcMillis() - startRtc;
    const uint32_t elapsedMs = millis() - startMs;
    if (elapsedMs == 0) return false;
    const float factor = float(elapsedRtc) / float(elapsedMs);
    if (!SleepTiming::validFactor(factor)) return false;
    setTimeCorrectionFactor(factor);
    return true;
}

void STM32LowPowerCal::sleepUntil(uint32_t startRtcMs, uint32_t delayMs, uint32_t leadMs)
{
    const uint32_t ticks = SleepTiming::remaining(startRtcMs, rtcMillis(), delayMs,
                                                 leadMs, time_Correction_Factor);
    if (ticks != 0) LowPower.deepSleep(ticks);
}

STM32LowPowerCal LowPowerCal;

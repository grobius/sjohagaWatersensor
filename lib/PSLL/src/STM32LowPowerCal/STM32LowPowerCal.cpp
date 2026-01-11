/*
@author Leo Korbee
@file   STM32LowPowerCal.cpp
@brief  Wrapper for LowPower - Correction for using LowPower in sleep modes with no crystal
time_correction factor is the factor that the millis time should be devided by to get the correct time.
and the correct time should be multiplied by to get millis internal to execute sleep
*/

#include "STM32LowPower.h"
#include "STM32LowPowerCal.h"

// correction times for time used by powerdown and wakeup.
// substract for correction
void STM32LowPowerCal::deepSleep(uint32_t ms, int substraction_time)
{
    int corrected_sleep_time = round((float) ms * LowPowerCal.time_Correction_Factor) - substraction_time;
    // call superclass instance deepsleep methode
    LowPower.deepSleep(corrected_sleep_time);
}

float STM32LowPowerCal::getTimeCorrectionFactor()
{
    return time_Correction_Factor;
}

void STM32LowPowerCal::setTimeCorrectionFactor(float time_Correction_Factor)
{
    LowPowerCal.time_Correction_Factor = time_Correction_Factor;
}

STM32LowPowerCal LowPowerCal;
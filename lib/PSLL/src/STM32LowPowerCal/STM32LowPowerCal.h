/*
@author Leo Korbee
@file   STM32LowPowerCal.h
@brief  Correction for usign LowPower in sleep modes with no crystal
*/

#include "STM32LowPower.h"

#ifndef _STM32_LOW_POWER_CAL_H_
#define _STM32_LOW_POWER_CAL_H_

class STM32LowPowerCal : public STM32LowPower {

    public:
        void deepSleep(uint32_t ms, int substraction_Time);
        void deepSleep(int ms, int substraction_Time)
        {
            deepSleep((uint32_t)ms, substraction_Time);
        }
       
        float getTimeCorrectionFactor();
        void setTimeCorrectionFactor(float time_Correction_Factor);
        bool calibrateRTC(uint32_t sampleMs = 8000);
        uint32_t rtcMillis();
        void sleepUntil(uint32_t startRtcMs, uint32_t delayMs, uint32_t leadMs);

         // declare timing parameters
        float time_Correction_Factor = 1.0f;
};

extern STM32LowPowerCal LowPowerCal;

#endif // _STM32_LOW_POWER_H_

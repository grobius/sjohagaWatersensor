/*
    Renamed to LoRaWAN.h
    Updated by Leo Korbee | 2022

    lorawan-arduino-rfm.h
    Copyright © 2018 lora.id. All right reserved.

    Use of this source code is governed by the MIT license that can be found in the LICENSE file.
*/

#ifndef _LORAWAN_H_
#define _LORAWAN_H_

#include <SPI.h>
#include <Arduino.h>
#include "AES-128.h"
#include "Encrypt.h"
#include "RFM95.h"
#include "LoRaMAC.h"
#include "Struct.h"
#include "Config.h"

/*
********************************************************************************************
* TYPE DEFINITION
********************************************************************************************
*/


#define LORAWAN_VERSION "1.0.0"

// delay times on join in seconds
#define JOIN_ACCEPT_DELAY1 5
#define JOIN_ACCEPT_DELAY2 6
/*
*****************************************************************************************
* CLASS
*****************************************************************************************
*/

class LoRaWANClass
{
    public:

        LoRaWANClass();
        ~LoRaWANClass();
        
        bool init(void);
        bool join(void);
        void setDeviceClass(devclass_t dev_class);
        // OTAA credentials
        void setDevEUI(const char *devEUI_in);
        void setAppEUI(const char *appEUI_in);
        void setAppKey(const char *appKey_in);
        // ABP credentials
        void setNwkSKey(const char *NwkKey_in);
        void setAppSKey(const char *ApskKey_in);
        void setDevAddr(const char *devAddr_in);
        void setUplinkData(char *data, unsigned int len, unsigned char confirm, unsigned char mport);
        void setDataRate(unsigned char data_rate);
        void setChannel(unsigned char channel);
        unsigned char getChannel();
        unsigned char getDataRate();
        void setTxPower1(unsigned char power_idx);
        void setTxPower(int level,txPin_t pinTx);
        int getRssi();
        int readData(char *outBuff);
        // Binary receive: returns length, or -1 if the buffer is too small.
        // Always consumes the pending message; never appends a terminator.
        int readData(uint8_t *outBuff, size_t capacity, uint8_t &port);
        bool readAck(void);
        void cycle(void);

        // frame counter
        unsigned int getFrameCounter();
        void setFrameCounter(unsigned int FrameCounter);

        // setters
        void setReceiveDelay1(unsigned char ReceiveDelay1);
        void sleepLowPower();

    private:
        void randomChannel();

    private:        
        // Messages
        unsigned char Data_Tx[MAX_UPLINK_PAYLOAD_SIZE];
        sBuffer Buffer_Tx;
        unsigned char Data_Rx[MAX_DOWNLINK_PAYLOAD_SIZE];
        sBuffer Buffer_Rx;
        sLoRa_Message Message_Control;

        // Declare ABP session
        unsigned char Address_Tx[4];
        unsigned char NwkSKey[16];
        unsigned char AppSKey[16];
        unsigned int Frame_Counter_Tx;
        sLoRa_Session Session_Data;

        // Declare OTAA data struct
        unsigned char DevEUI[8];
        unsigned char AppEUI[8];
        unsigned char AppKey[16];
        unsigned char DevNonce[2];
        unsigned char AppNonce[3];
        unsigned char NetID[3];
        sLoRa_OTAA OTAA_Data;

        // Declare LoRA settings struct
        sSettings LoRa_Settings;
        sRFM_pins LoRa_Pins;

        unsigned char drate_common;

        // Lora Setting Class
        devclass_t dev_class;

        // channel mode
        unsigned char currentChannel;

        // UART
        RFM_command_t RFM_Command_Status;
        rx_t Rx_Status;

        // ACK reception
        ack_t Ack_Status;
};



#endif

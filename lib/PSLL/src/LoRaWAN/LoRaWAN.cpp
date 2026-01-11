/*
    renamed to LoRaWAN.cpp
    Updated/improved by Leo korbee | 2025

*/

/*
    loraid-arduino-rfm.cpp
    Copyright © 2018 lora.id. All right reserved.

    Author: Andri Rahmadhani
    Date: 2018-04-25

    Encapsulate Ideetron LoRaWAN simple node demonstrator
    This firmware supports
        *Over The Air Activation
        *Activation By Personalization
        *Class switching between Class A and Class C motes
        *Channel hopping

    The following settings can be done
        *Channel Receive and Transmit
        *Datarate Receive and Transmit
        *Transmit power
        *Confirmed or unconfirmed messages
        *Device Address
        *Application Session Key
        *Network Session Key
        *Device EUI
        *Application EUI
        *Application key
        *Mote Class

    Use of this source code is governed by the MIT license that can be found in the LICENSE file.
*/


#include "LoRaWAN.h"
#include "Conversions.h"
#include "../STM32LowPowerCal/STM32LowPowerCal.h"

// constructor
LoRaWANClass::LoRaWANClass()
{
}

// destructor
LoRaWANClass::~LoRaWANClass()
{
}

// initialisation 
bool LoRaWANClass::init(void)
{
    /**** default settings ****/

    // lora Default Setting Class
    dev_class = CLASS_A;
 
    // current channel
    currentChannel = MULTI;

    /**** default LoRa settings ****/

    // Device Class
    LoRa_Settings.Mote_Class = CLASS_A; //0x00 is type A, 0x01 is type C

    // Rx and Tx settings for different regions
#if defined(EU_868)
    LoRa_Settings.Datarate_Rx = SF7BW125;   //set to SF7 BW 125 kHz
    LoRa_Settings.Datarate_Rx2 = SF9BW125;  //set to SF9 BW 125 kHz for TTN (default is SF12BW125)
    LoRa_Settings.Datarate_Tx = drate_common = SF7BW125;   //set to SF7 BW 125 kHz
#elif defined(AU_915)
    LoRa_Settings.Datarate_Rx = SF8BW125;   //set to SF8 BW 500 kHz
    LoRa_Settings.Datarate_Tx = drate_common = SF7BW125;   //set to SF7 BW 125 kHz
#elif defined(US_915)
    LoRa_Settings.Datarate_Rx = SF8BW125;   //set to SF8 BW 500 kHz
    LoRa_Settings.Datarate_Tx = drate_common = SF7BW125;   //set to SF7 BW 125 kHz
#elif defined(AS_923)
    LoRa_Settings.Datarate_Rx = SF10BW125;   //set to SF10 BW 125 kHz
    LoRa_Settings.Datarate_Tx = drate_common = SF12BW125;   //set to SF12 BW 125 kHz
#endif

    LoRa_Settings.Channel_Rx = 0x08;    // set to recv channel
    LoRa_Settings.Channel_Tx = 0x00;    // set to channel 0

    LoRa_Settings.Confirm = 0x00; //0x00 unconfirmed, 0x01 confirmed
    LoRa_Settings.Channel_Hopping = 0x00; //0x00 no channel hopping, 0x01 channel hopping

    // default settings for Receive_Delay1 and Receive_Delay2
    LoRa_Settings.Recieve_Delay1 = 1;
    LoRa_Settings.Recieve_Delay2 = LoRa_Settings.Recieve_Delay1 + 1;

    // DEBUG_PRINT("Setting Tx at init ");
    // DEBUG_PRINTLN(LoRa_Settings.Datarate_Tx, HEX);

    /**** Initialize ****/

    // initialize Status
    RFM_Command_Status = NO_RFM_COMMAND;
    Rx_Status = NO_RX;
    Ack_Status = NO_ACK;

    // Initialise session data struct (Semtech default key)
    memset(Address_Tx, 0x00, 4);
    memset(NwkSKey, 0x00, 16);
    memset(AppSKey, 0x00, 16);

    // alway start with framcounter = 1
    Frame_Counter_Tx = 0x0001;

    Session_Data.NwkSKey = NwkSKey;
    Session_Data.AppSKey = AppSKey;
    Session_Data.DevAddr = Address_Tx;
    Session_Data.Frame_Counter = &Frame_Counter_Tx;

    //Initialize OTAA data struct
    memset(DevEUI, 0x00, 8);
    memset(AppEUI, 0x00, 8);

    memset(AppKey, 0x00, 16);
    memset(DevNonce, 0x00, 2);
    memset(AppNonce, 0x00, 3);
    memset(NetID, 0x00, 3);

    OTAA_Data.DevEUI = DevEUI;
    OTAA_Data.AppEUI = AppEUI;
    OTAA_Data.AppKey = AppKey;
    OTAA_Data.DevNonce = DevNonce;
    OTAA_Data.AppNonce = AppNonce;
    OTAA_Data.NetID = NetID;

    // Initialise buffer for data to transmit
    memset(Data_Tx, 0x00, sizeof(Data_Tx));
    Buffer_Tx.Data = Data_Tx;
    Buffer_Tx.Counter = 0x00;

    // Initialise buffer for data to receive
    memset(Data_Rx, 0x00, sizeof(Data_Rx));
    Buffer_Rx.Data = Data_Rx;
    Buffer_Rx.Counter = 0x00;
    Message_Control.Direction = 0x01; //Set down direction for Rx message

    //Initialize I/O pins
    pinMode(RFM_pins.DIO0,INPUT);
    pinMode(RFM_pins.DIO1,INPUT);
    #ifdef BOARD_DRAGINO_SHIELD
    pinMode(RFM_pins.DIO5,INPUT);
    #endif
    pinMode(RFM_pins.DIO2,INPUT);
    pinMode(RFM_pins.CS,OUTPUT);
    pinMode(RFM_pins.RST,OUTPUT);

    digitalWrite(RFM_pins.CS,HIGH);

    // Reset
    digitalWrite(RFM_pins.RST,HIGH);
    delay(10);
    digitalWrite(RFM_pins.RST,LOW);
    delay(10);
    digitalWrite(RFM_pins.RST,HIGH);

    
    /*** This prevents the use of other SPI devices with different settings ***/
    //SPI.beginTransaction(SPISettings(4000000,MSBFIRST,SPI_MODE0));

    //Wait until RFM module is started
    delay(5);

    //Initialize RFM module
    if(!RFM_Init())
    {
      return 0;
    }

    return 1;
}

bool LoRaWANClass::join(void)
{
    bool join_status;
    // x seconds timeout converted to millis, the join message should be received in RX1_Join window!
    //int Join_Accept_Delay_1 = round(JOIN_ACCEPT_DELAY1 * 1000 * LoRa_Settings.Time_Correction);
    int Join_Accept_Delay_1 = JOIN_ACCEPT_DELAY1 * 1000;

    // timeout of 6 seconds, then you should have received data 
    unsigned int timeout = Join_Accept_Delay_1 + 1000;
    unsigned long prev_millis;

    if (currentChannel == MULTI)
    {
        // for join only channel 0, 1, and 2 can be used
        do
        {       
            randomChannel();
        } while (LoRa_Settings.Channel_Tx > 2);
    }

    // join request
    LoRa_Send_JoinReq(&OTAA_Data, &LoRa_Settings);
    prev_millis = millis();
    // set RFM95 module to low power
    // this->sleepLowPower();
    // LowPowerCal.begin();
    // 40 ms = 26.8 go to sleep RFM + 11.3 wakeup RFM + 40 ms extra -> time should be about 5920-5930
    // LowPowerCal.deepSleep(Join_Accept_Delay_1, 80); 
    
    // delay to reduce power during waiting
    // delay(timeout-1000);
    // loop for <timeout> wait for join accept
    // normal 50, when debugging 100
    delay(Join_Accept_Delay_1 - 50);

    do
    {
        join_status = LORA_join_Accept(&Buffer_Rx, &Session_Data, &OTAA_Data, &Message_Control, &LoRa_Settings);   
    }
    while (((millis() - prev_millis) < timeout) && !join_status);
    // with time correction between airtime and processing signal
    unsigned int joinmillis = millis() - prev_millis - 180;
    
    
    DEBUG_PRINT("joinmillis: ");
    DEBUG_PRINTLN(joinmillis);
    // put the correction factor in the LowPowerCal
    LowPowerCal.setTimeCorrectionFactor((float) Join_Accept_Delay_1/joinmillis);
    
    return join_status;
}

void LoRaWANClass::setDevEUI(const char *devEUI_in)
{
    for(byte i = 0; i < 8; ++i)
        DevEUI[i] = ASCII2Hex(devEUI_in[i*2],devEUI_in[(i*2) + 1]);
    //Reset frame counter
    Frame_Counter_Tx = 0x0001;

    //Reset RFM command status
    RFM_Command_Status = NO_RFM_COMMAND;
}

void LoRaWANClass::setAppEUI(const char *appEUI_in)
{
    for(byte i = 0; i < 8; ++i)
        AppEUI[i] = ASCII2Hex(appEUI_in[i*2],appEUI_in[(i*2) + 1]);
    //Reset frame counter
    Frame_Counter_Tx = 0x0001;

    //Reset RFM command status
    RFM_Command_Status = NO_RFM_COMMAND;
}

void LoRaWANClass::setAppKey(const char *appKey_in)
{
    for(byte i = 0; i < 16; ++i)
        AppKey[i] = ASCII2Hex(appKey_in[i*2],appKey_in[(i*2) + 1]);
    //Reset frame counter
    Frame_Counter_Tx = 0x0001;

    //Reset RFM command status
    RFM_Command_Status = NO_RFM_COMMAND;
}

void LoRaWANClass::setNwkSKey(const char *NwkKey_in)
{
    for (uint8_t i = 0; i < 16; ++i)
        NwkSKey[i] = ASCII2Hex(NwkKey_in[i*2],NwkKey_in[(i*2)+1]);

    //Reset frame counter
    Frame_Counter_Tx = 0x0001;

    //Reset RFM command status
    RFM_Command_Status = NO_RFM_COMMAND;
}

void LoRaWANClass::setAppSKey(const char *ApskKey_in)
{
    for (uint8_t i = 0; i < 16; ++i)
        AppSKey[i] = ASCII2Hex(ApskKey_in[i*2],ApskKey_in[(i*2)+1]);

    //Reset frame counter
    Frame_Counter_Tx = 0x0001;

    //Reset RFM command status
    RFM_Command_Status = NO_RFM_COMMAND;
}

void LoRaWANClass::setDevAddr(const char *devAddr_in)
{
    memset(Session_Data.DevAddr, 0x30, sizeof(Session_Data.DevAddr));

    //Check if it is a set command and there is enough data sent
    Address_Tx[0] = ASCII2Hex(devAddr_in[0],devAddr_in[1]);
    Address_Tx[1] = ASCII2Hex(devAddr_in[2],devAddr_in[3]);
    Address_Tx[2] = ASCII2Hex(devAddr_in[4],devAddr_in[5]);
    Address_Tx[3] = ASCII2Hex(devAddr_in[6],devAddr_in[7]);

    //Reset frame counter
    Frame_Counter_Tx = 0x0001;

    //Reset RFM command status
    RFM_Command_Status = NO_RFM_COMMAND;
}

void LoRaWANClass::setTxPower(int level,txPin_t pinTx)
{
    RFM_Set_Tx_Power(level, pinTx);
}

int LoRaWANClass::getRssi()
{
    // return rssi value in dBm - convertion according to sx1276 datasheet
    return -157 + RFM_Get_Rssi();
}

void LoRaWANClass::setDeviceClass(devclass_t dev_class)
{
    switch (dev_class)
    {
        case CLASS_A:
            LoRa_Settings.Mote_Class = CLASS_A;
            break;
        case CLASS_A_NO_RX:
            LoRa_Settings.Mote_Class = CLASS_A_NO_RX;
            break;
        case CLASS_C:
            LoRa_Settings.Mote_Class = CLASS_C;
            break;
        default:
            LoRa_Settings.Mote_Class = CLASS_A;
    }
 
    if (LoRa_Settings.Mote_Class == CLASS_A || LoRa_Settings.Mote_Class == CLASS_A_NO_RX) 
    {
        RFM_Switch_Mode(RFM_MODE_STANDBY);
    } 
    else 
    {
        RFM_Continuous_Receive(&LoRa_Settings);
    }

    //Reset RFM command
    RFM_Command_Status = NO_RFM_COMMAND;
}

void LoRaWANClass::setUplinkData(char *data, unsigned int len, unsigned char confirm, unsigned char mport)
{
    if (currentChannel == MULTI) {
        randomChannel();
    }
    LoRa_Settings.Confirm = (confirm == 0) ? 0 : 1;
    if (mport == 0) mport = 1;
    if (mport > 223) mport = 1;
    LoRa_Settings.Mport = mport;
    //Set new command for RFM
    RFM_Command_Status = NEW_RFM_COMMAND;
    Buffer_Tx.Counter = len;
    memcpy(Buffer_Tx.Data,data,len);
}

void LoRaWANClass::setDataRate(unsigned char data_rate)
{
    drate_common = data_rate;
#if defined(US_915)
  if(drate_common <= 0x04){
    LoRa_Settings.Datarate_Tx = drate_common;
    LoRa_Settings.Datarate_Rx = data_rate + 0x0A;
  }
#elif defined(AU_915)
  if(drate_common <= 0x04){
    LoRa_Settings.Datarate_Tx = drate_common;
    LoRa_Settings.Datarate_Rx = data_rate + 0x0A;
  }
#else
  //Check if the value is oke
  if(drate_common <= 0x06)
  {
    LoRa_Settings.Datarate_Tx = drate_common;
    LoRa_Settings.Datarate_Rx = drate_common;
  }

#endif
    RFM_Command_Status = NO_RFM_COMMAND;
}

void LoRaWANClass::setChannel(unsigned char channel)
{
    if (channel <= 7) 
    {
        currentChannel = channel;
        LoRa_Settings.Channel_Tx = channel;
#ifdef US_915
        LoRa_Settings.Channel_Rx = channel + 0x08;
#elif defined(AU_915)
        LoRa_Settings.Channel_Rx = channel + 0x08;
#elif defined(EU_868)
        LoRa_Settings.Channel_Rx = channel;
#endif
    } else if (channel == MULTI) 
    {
        currentChannel = MULTI;
    }
}

unsigned char LoRaWANClass::getChannel()
{
    return LoRa_Settings.Channel_Tx;
}

unsigned char LoRaWANClass::getDataRate() 
{
    return LoRa_Settings.Datarate_Tx;
}

void LoRaWANClass::setTxPower1(unsigned char power_idx)
{
    unsigned char RFM_Data;
    LoRa_Settings.Transmit_Power = (power_idx > 0x0F) ? 0x0F : power_idx;
    // PA_BOOST pin used = YES
    RFM_Data = LoRa_Settings.Transmit_Power + 0xF0;

    // RegPaConfig => see RFM95W datasheet
    // bit 7   -> 0 = RFO_PIN Output power is limited to -4 -> +14dBm on 50ohms, 1 = PA_BOOST_PIN, Ouput power is limited to +20dBm
    // bit 6-4 -> Select max output power: Pmax=10.8+0.6*MaxPower[dBm]
    // bit 3-0 -> OutputPower: Pout=Pmax-(15-OutputPower) if PaSelect = 0 (RFO pin) | Pout=17-(15-OutputPower) if PaSelect = 1 (PA_BOOST pin)
    
    // from sources from internet it seams the RMF95W modules uses the PA_BOOST pin and this is connected to the ANT pin.
    // So power is set from 17-2dBm (15-0 dBm by Pmax calculations (10.8 + 7*0.6)
    // regulation = max 14dBm = 25mW with 1% duty cycle.

    RFM_Write(RFM_REG_PA_CONFIG, RFM_Data);
}

int LoRaWANClass::readData(char *outBuff)
{
    int res = 0;
    //If there is new data
    if(Rx_Status == NEW_RX)
    {
        res = Buffer_Rx.Counter;
        memset(outBuff, 0x00, res + 1);
        memcpy(outBuff, Buffer_Rx.Data, res);

        // Clear Buffer counter
        Buffer_Rx.Counter = 0x00;

        Rx_Status = NO_RX;
    }

    return res;
}

/**
 * Get ACK flag from downlink packet
 *
 * @return true in case of ACK received
 */
bool LoRaWANClass::readAck(void)
{
    if (Ack_Status == NEW_ACK)
    {
      Ack_Status = NO_ACK;
      return true;
    }
    return false;
}

void LoRaWANClass::cycle(void)
{
    //Type A mote transmit receive cycle
    if((RFM_Command_Status == NEW_RFM_COMMAND || RFM_Command_Status == JOIN) && (LoRa_Settings.Mote_Class == CLASS_A || LoRa_Settings.Mote_Class == CLASS_A_NO_RX) )
    {
        //LoRaWAN TX/RX cycle
        // Before Cycle -> Message_Rx is messagecontrol that is send
        LORA_Cycle(&Buffer_Tx, &Buffer_Rx, &RFM_Command_Status, &Session_Data, &OTAA_Data, &Message_Control, &LoRa_Settings);

        // check acknowledge bit
        if ((Message_Control.Frame_Control & 0x20) > 0)
            Ack_Status = NEW_ACK;

        // original length of frame options list
        unsigned char Frame_Options_Lenght = Message_Control.Frame_Control & 0x0F;

        if (Frame_Options_Lenght > 0)
        {
            DEBUG_PRINTLN("LoRaWAN: Frame Options available");
            // create newoptions array, max 15 octets
            unsigned char newoptions[15]; 

            // loop through all frame options and create new options...
            for (int i = 0; i < Frame_Options_Lenght; i++)
            {
                switch (Message_Control.Frame_Options[i])
                {
                    case 0x05:
                        DEBUG_PRINTLN("MacCommand 0x05: RXParamSetupReq");
                        // command
                        newoptions[i+0] = 0x05;
                        // All Paramesetup are succesfull: b00000111
                        newoptions[i+1] = 0x07;
                        Message_Control.Frame_Control += 2;
                        break;

                    case 0x06:
                        DEBUG_PRINTLN("MacCommand 0x06: DevStatusReq");
                        // command
                        newoptions[i+0] = 0x06;
                        // battery level
                        newoptions[i+1] = 0xFF;
                        // SNR level
                        newoptions[i+2] = 0x56;
                        // add number of bytes to Message.Frame_Control
                        Message_Control.Frame_Control += 3;
                        break;	
                        
                    default: 
                        // mac command not found, but printed for debugging
                        DEBUG_PRINT("MacCommand ");
                        DEBUG_PRINTLN(Message_Control.Frame_Options[i], HEX);
                }
            }

            // put new options into frame options
            for (int i = 0; i < Frame_Options_Lenght; i++)
            {
                Message_Control.Frame_Options[i] = newoptions[i];
            }
        }

        // 101x 0xA0 Confirmed data Down
        if (Message_Control.MAC_Header == 0xA0)
        {
            DEBUG_PRINTLN("LoRaWAN: Confirmed downlink message");
            // set ack bit for next transmission
            Message_Control.Frame_Control |= 0x20;
        }

        // check buffer recived
        if(Buffer_Rx.Counter != 0x00)
        {
            Rx_Status = NEW_RX;
        }

        // reset status
        RFM_Command_Status = NO_RFM_COMMAND;
    }

    //Type C mote transmit and receive handling
    if(LoRa_Settings.Mote_Class == CLASS_C)
    {
        //Transmit
        if(RFM_Command_Status == NEW_RFM_COMMAND)
        {
            //LoRaWAN TX/RX cycle
            LORA_Cycle(&Buffer_Tx, &Buffer_Rx, &RFM_Command_Status, &Session_Data, &OTAA_Data, &Message_Control, &LoRa_Settings);
            if(Buffer_Rx.Counter != 0x00)
            {
                Rx_Status = NEW_RX;
            }
            RFM_Command_Status = NO_RFM_COMMAND;
        }

        //Receive
        if(digitalRead(RFM_pins.DIO0) == HIGH)
        {
            LORA_Receive_Data(&Buffer_Rx, &Session_Data, &OTAA_Data, &Message_Control, &LoRa_Settings);
            if(Buffer_Rx.Counter != 0x00)
            {
                Rx_Status = NEW_RX;
            }
        }
        RFM_Command_Status = NO_RFM_COMMAND;
    }

}

void LoRaWANClass::randomChannel()
{
    randomSeed(analogRead(ANALOGINPUTFORRANDOMIZING));
    unsigned char freq_idx;
#ifdef AS_923
    freq_idx = random(0,9);
    // limit drate, ch 8 -> sf7bw250
    LoRa_Settings.Datarate_Tx = freq_idx == 0x08? 0x06 : drate_common;
#elif defined(EU_868)
    freq_idx = random(0,7);
    LoRa_Settings.Channel_Rx=freq_idx;      // same rx and tx channel
#else // US_915 or AU_915
    freq_idx = random(0,8);
    LoRa_Settings.Channel_Rx = freq_idx + 0x08;
#endif
    LoRa_Settings.Channel_Tx = freq_idx;
}

uint32_t LoRaWANClass::getFrameCounter() 
{
    return Frame_Counter_Tx;
}

void LoRaWANClass::setFrameCounter(uint32_t FrameCounter) 
{
    Frame_Counter_Tx = FrameCounter;
}

void LoRaWANClass::setReceiveDelay1(unsigned char RecieveDelay1)
{
    LoRa_Settings.Recieve_Delay1 = RecieveDelay1;
    LoRa_Settings.Recieve_Delay2 = RecieveDelay1 + 1;
}

/*
    set RFM module to sleep and reduce power by setting some pins to HIGH connected to RFM95 module
*/ 
void LoRaWANClass::sleepLowPower()
{
    RFM_Mode_Sleep_LowPower();
}

// define lora objet
LoRaWANClass lora;

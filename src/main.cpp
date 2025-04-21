/**
 * Example and test of OTAA device connection
 * Author: Leo Korbee
 * 2022-10-23
 * Tested on MiniPill LoRa (STM32L051C8T6 with RFM95W)
 * See end of file for connection diagram
 * 
 * Adapted main code with some improvements:
 * - Added DEBUG possibilities
 * - Added security feature/file - do not upload this one to your git-repository!
 * 
 * Adapted library for some improvements:
 * - Added ADR return message for 0x06 Request
 * - Improved DevNonce calculation/randomization
 * - Mac command processing
 * - Confirmed downlink works!
 * - Low Power mode, also during Join and Cycle (send and receive RX1 or RX2)
 * - RTC timing correction for Low Power mode
 * 
 * 2023-05-14
 * - set calTime in secconfig.h and used for calibration.
 * - secconfig should be renamed?
 * 
 * 2023-10-17
 * - added class CLASS_A_NO_RX for devices that only sends data and do not use the 
 *   receive window to reduce power. Updated LoRaMAC.cpp
 * 
 * 2024-06-17
 * - changed secconfig to config.h
 */
//#include <Arduino.h>
#include "lorawan.h"
// include for security credentials
#include "config.h"
#include "STM32LowPowerCal.h"

// include for internal voltage reference
#include "STM32IntRef.h"

// ds18b20 libraries

#include <OneWire.h>
#include <DallasTemperature.h>

// Data wire is plugged into port PA5 on the MiniPill
#define ONE_WIRE_BUS PA1

// Setup a oneWire instance to communicate with any OneWire devices (not just Maxim/Dallas temperature ICs)
OneWire oneWire(ONE_WIRE_BUS);

// Pass our oneWire reference to Dallas Temperature.
DallasTemperature sensors(&oneWire);

// arrays to hold device address
DeviceAddress insideThermometer;

// uncomment for debugging main
//#define DEBUG_MAIN

// Define constants for magic numbers
#define BAUD_RATE 9600
#define RETRY_DELAY_MS 15000
#define SLEEP_INTERVAL_US 600000
#define TEMP_OFFSET 50

#ifdef DEBUG_MAIN
  // Correctly define Serial2 for STM32
  HardwareSerial Serial2(PA2, PA3); // TX on PA2, RX on PA3
  #define DEBUG_INIT() Serial2.begin(BAUD_RATE)
  #define DEBUG_PRINT(...) Serial2.print(__VA_ARGS__)
  #define DEBUG_PRINTLN(...) Serial2.println(__VA_ARGS__)
#else
  #define DEBUG_INIT()
  #define DEBUG_PRINT(...)
  #define DEBUG_PRINTLN(...)
#endif

/* A BME280 object using SPI, chip select pin PA1 */
//BME280 bme(SPI,PA1);

// Sleep this many microseconds. Notice that the sending and waiting for downlink
// will extend the time between send packets. You have to extract this time
#define SLEEP_INTERVAL 600000

char outStr[255];
byte recvStatus = 0;

// see connection diagram at the end of this file
// MOSI pin is used for lowPower settings on RFM module
const sRFM_pins RFM_pins =
{
  .MOSI = PA7,
  .CS = PA4,
  .RST = PA9,
  .DIO0 = PA10,
  .DIO1 = PB4,
  .DIO2 = PB5
//  .DIO5 = ,
};

void setup()
{ 
  // Setup serial debug
  DEBUG_INIT();
  // time for serial settings
  delay(1000);
  
 /************************************************
 * get TimeCorrection number for RTC times
 ************************************************/
  // wait 8 seconds to upload new code and
  // calibrate the RTC times with the HSI internal clock
  // time calibration on device with correct timing (ideal 8000.00)
  // incease this time to shorten time between send and receive
  LowPowerCal.setRTCCalibrationTime(calTimeDivider);
  // callibrate for 8 seconds
  LowPowerCal.calibrateRTC();
  DEBUG_PRINT("RTC Time Correction: ");
  DEBUG_PRINTLN(LowPowerCal.getRTCTimeCorrection(), 5);
  
  if(!lora.init())
  {
    DEBUG_PRINTLN("RFM95 not detected");
    delay(5000);
    return;
  }

  // Set LoRaWAN Class change CLASS_A or CLASS_C or CLASS_A_NO_RX
  lora.setDeviceClass(CLASS_A_NO_RX);

  // Set Data Rate for join!
  lora.setDataRate(SF7BW125);

  // Set Receive Delay for RX1 window
  lora.setReceiveDelay1(5);

  // set channel to random
  lora.setChannel(MULTI);

// Join channels:
// 868.10 MHz 868.30 MHz 868.50 MHz -> channel 0, 1, 2


  // use this setting for power settings lora.setTxPower will use boost pin?
  // power level between 0 and 16
  lora.setTxPower1(0);

  // Put OTAA Key and DevAddress here
  lora.setDevEUI(devEui);
  lora.setAppEUI(appEui);
  lora.setAppKey(appKey);
  
  // set framecounter for first packet to 1
  lora.setFrameCounter(1);

  // Join procedure
  // https://lora-developers.semtech.com/documentation/tech-papers-and-guides/the-book/joining-and-rejoining
  
  bool isJoined = false;
  do
  {
    DEBUG_PRINTLN("Joining...");
    isJoined = lora.join();
    // faster 
    if (isJoined) break;
    
    //wait for 15s to try again
    lora.sleepLowPower();
    LowPower.begin();
    LowPower.deepSleep(RETRY_DELAY_MS);

  }while(!isJoined);
  DEBUG_PRINTLN("Joined to network");

  // set dataRate for sending data instead of defaults
  lora.setDataRate(SF7BW125);

  // Start up the ds18b20 library
  sensors.begin();
  sensors.setResolution(10);
  // if (!sensors.getAddress(insideThermometer, 0)) Serial.println("Unable to find address for Device 0");

  // sensors.setResolution(insideThermometer, 9);



}

void loop()
{
  // Prepare upstream data transmission at the next possible time.
  uint8_t dataLength = 8;
  char data[dataLength];

  // read vcc and add to bytebuffer
  int32_t vcc = IntRef.readVref();
  data[0] = (vcc >> 8) & 0xff;
  data[1] = (vcc & 0xff);

  // reading data from ds18b20
  //sensors.requestTemperatures();
  sensors.requestTemperatures();
  float tempFloat = sensors.getTempCByIndex(0);

  if (tempFloat == DEVICE_DISCONNECTED_C)
  {
    DEBUG_PRINTLN("Error: Could not read temperature data");
    tempFloat = 999;
    return;
  }
  tempFloat = tempFloat + TEMP_OFFSET;    //Using 50degre offset to handle negative values. Should be subtracted in payload decoder

  // from float to uint16_t
  uint16_t tempInt = (100 * tempFloat);  


  // move BME data into bytebuffer
  data[2] = (tempInt >> 8) & 0xff;
  data[3] = tempInt & 0xff;

  // unconfirmed data (0), port (1)
  lora.setUplinkData(data, strlen(data), 0, 1);


  
  // cycle -> send (if new message available) and wait for response in RX1 and RX2
  lora.cycle();

  recvStatus = lora.readData(outStr);
  if(recvStatus)
  {
    DEBUG_PRINTLN(outStr);
    int i = 0;
    while(outStr[i] != '\0' && i < sizeof(outStr))
    {
        DEBUG_PRINT(outStr[i], HEX);
        DEBUG_PRINT(" ");
        i++;
    }
    DEBUG_PRINTLN();
  }

  DEBUG_PRINTLN("Going to long sleep");
  lora.sleepLowPower();
  
  LowPower.begin();
  delay(100);
  // set PA6 to analog to reduce power due to currect flow on DIO to BME280
  // done @ lora.sleepLowPower due to lower power during RX1/RX2 window
  // pinMode(PA6, INPUT_ANALOG);

  LowPower.deepSleep(SLEEP_INTERVAL_US);
  
}


// function to print a device address
void printAddress(DeviceAddress deviceAddress)
{
  for (uint8_t i = 0; i < 8; i++)
  {
    if (deviceAddress[i] < 16) Serial.print("0");
    Serial.print(deviceAddress[i], HEX);
  }
}

/* MiniPill LoRa v1.x mapping - LoRa module RFM95W and BME280 sensor
  PA1  //            NSS           - BME280
  PA4  // SPI1_NSS   NSS  - RFM95W
  PA5  // SPI1_SCK   SCK  - RFM95W - BME280
  PA6  // SPI1_MISO  MISO - RFM95W - BME280
  PA7  // SPI1_MOSI  MOSI - RFM95W - BME280

  PA10 // USART1_RX  DIO0 - RFM95W
  PB4  //            DIO1 - RFM95W
  PB5  //            DIO2 - RFM95W

  PA9  // USART1_TX  RST  - RFM95W

  VCC - 3V3
  GND - GND
*/

/**
 * Example and test of OTAA device connection
 * Author: Leo Korbee
 * 2022-10-23
 * Tested on MiniPill LoRa (STM32L051C8T6 with RFM95W)
 * See end of file for connection diagram
 * 
 * @2025-09-30 updated for multiple temperature sensors
 */

// for MCP9808 communication
#include <Wire.h>
// include PSLL library
#include "lorawan.h"
// includes for security credentials
#include "config_security.h"
// include for internal voltage reference
#include "STM32IntRef.h"

/************************************************
 * Settings for MCP9808 temperature sensor
 ************************************************/
#include "Adafruit_MCP9808.h"
// Create the MCP9808 temperature sensor object
Adafruit_MCP9808 tempsensor = Adafruit_MCP9808();
byte baseAddress = 0x18;
#define maxNumberOfSensors 2
// Index (bit gives the on/off per index)
uint8_t sensorIndex = 0x00;
Adafruit_MCP9808 sensorList[maxNumberOfSensors];

// uncomment for debugging main
#define DEBUG_MAIN

#ifdef DEBUG_MAIN
  // for debugging redirect to hardware Serial2
  // Tx on PA2 - connect your Rx to PA2 and baudrate 115200,8N1
  #define DEBUG_INIT(...) HardwareSerial Serial2(USART2)   // or HardWareSerial Serial2(PA3, PA2);
  #define DEBUG_BEGIN(...) Serial2.begin(115200)
  #define DEBUG_PRINT(...) Serial2.print(__VA_ARGS__)
  #define DEBUG_PRINTLN(...) Serial2.println(__VA_ARGS__)
  DEBUG_INIT();
#else
  #define DEBUG_BEGIN(...)
  #define DEBUG_PRINT(...)
  #define DEBUG_PRINTLN(...)
#endif

// Sleep this many microseconds. Notice that the sending and waiting for downlink
// will extend the time between send packets. You have to extract this time
#define SLEEP_INTERVAL 300000

// received data and status
char recStr[255];
byte receivedStatus = 0;

// see connection diagram at the end of this file
// MOSI pin is used to get lowPower settings on RFM module
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

/**
 * Function prototype
 */
void init_sensors();
// void read_sensors();

void setup()
{ 
  // Setup serial debug
  DEBUG_BEGIN();
  // time for serial settings
  delay(500);
  DEBUG_PRINTLN("Setup starting");

  /************************************************
  * LoRa init and set default parameters
  ************************************************/
  if(!lora.init())
  {
    DEBUG_PRINTLN("RFM95 not detected");
    delay(5000);
    return;
  }

  // Set LoRaWAN Class change CLASS_A, CLASS_A_NO_RX or CLASS_C 
  lora.setDeviceClass(CLASS_A);
  // Set Data Rate for join!
  lora.setDataRate(SF7BW125);
  // Set Receive Delay for RX1 window
  lora.setReceiveDelay1(5);
  // set channel to random
  lora.setChannel(MULTI);
  // use this setting for power settings
  // power level between 0 and max 15 0dBm -> 15 dBm?
  lora.setTxPower1(TX_0dBm);
  // Put OTAA Key and DevAddress here from secconfig.h file
  lora.setDevEUI(devEui);
  lora.setAppEUI(appEui);
  lora.setAppKey(appKey);

  /************************************************
  * Setup I2C and MCP9808 Temperature sensor
  *************************************************/
  Wire.begin();
  init_sensors();

  /************************************************
  * LoRa Start Join Process
  ************************************************/
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
    LowPower.deepSleep(15000);

  }while(!isJoined);
  DEBUG_PRINTLN("Joined to network");

  DEBUG_PRINT("Time Correction: ");
  DEBUG_PRINTLN(LowPowerCal.getTimeCorrectionFactor(), 5);
}

void loop()
{
  /************************************************
  * Set data ready for sending with LoRa
  ************************************************/

  uint8_t dataLength = 3 + (maxNumberOfSensors * 2);
  char data[dataLength];

  int dataIndex = 0;

  // read vcc and add to bytebuffer
  int32_t vcc = IntRef.readVref();
  data[dataIndex++] = (vcc >> 8) & 0xff;
  data[dataIndex++] = (vcc & 0xff);
  
  // put sensorIndex in next byte
  data[dataIndex++] = sensorIndex;

  // read sensors an put data in buffer
  for (uint8_t addr=0; addr < maxNumberOfSensors; addr++)
  {
    if ((0x01 << addr) & sensorIndex)
    {
      sensorList[addr].wake();
      // time to wake up
      delay(20);
      float temperature = sensorList[addr].readTempC();
      // printf("(0x%02X) Temp: %.1f *C\n", baseAddress+addr, temperature); 
      sensorList[addr].shutdown();
      // put it in data-array;
      int tempInt =  temperature * 100;
      data[dataIndex++] = (tempInt >> 8) & 0xFF;
      data[dataIndex++] = (tempInt) & 0xFF;
    }
  }
  
  DEBUG_PRINT("DATALENGHT: ");
  DEBUG_PRINTLN(strlen(data));
  DEBUG_PRINT("Frame Counter: ");
  DEBUG_PRINTLN(lora.getFrameCounter());

  lora.setUplinkData(data, dataIndex, 0, 1);

  /************************************************
  * LoRa Send data and wait for received data
  ************************************************/
  // cycle -> send (if new message available) and wait for response in RX1 and RX2
  lora.cycle();

  /************************************************
  * Check if data is received and print
  ************************************************/
  receivedStatus = lora.readData(recStr);
  if(receivedStatus)
  {
    DEBUG_PRINTLN(recStr);
    int i = 0;
    while(recStr[i] != '\0' && i < sizeof(recStr))
    {
        DEBUG_PRINT(recStr[i], HEX);
        DEBUG_PRINT(" ");
        i++;
    }
    DEBUG_PRINTLN();
  }
  /************************************************
  * Power down and going to long sleep
  ************************************************/
  
  DEBUG_PRINTLN("Going to long sleep");
  lora.sleepLowPower();
  LowPowerCal.begin();
  LowPowerCal.deepSleep(SLEEP_INTERVAL, 0);
}

void init_sensors()
{
  DEBUG_PRINTLN("Init sensors");
  // Make sure the sensor is found, you can also pass in a different i2c
  // address with tempsensor.begin(0x19) for example, also can be left in blank for default address use
  // Also there is a table with all addres possible for this sensor, you can connect multiple sensors
  // to the same i2c bus, just configure each sensor with a different address and define multiple objects for that
  //  A2 A1 A0 address
  //  0  0  0   0x18  this is the default address
  //  0  0  1   0x19
  //  0  1  0   0x1A
  //  0  1  1   0x1B
  //  1  0  0   0x1C
  //  1  0  1   0x1D
  //  1  1  0   0x1E
  //  1  1  1   0x1F

  for (uint8_t addr=0; addr < maxNumberOfSensors; addr++)
  {
    sensorList[addr] = Adafruit_MCP9808();

    if (sensorList[addr].begin(baseAddress + addr))
    {
      
      // printf("sensor connected to 0x%02X\n", baseAddress+ad);
      sensorList[addr].setResolution(3); // sets the resolution mode of reading, the modes are defined in the table bellow:
      // Mode Resolution SampleTime
      //  0    0.5°C       30 ms
      //  1    0.25°C      65 ms
      //  2    0.125°C     130 ms
      //  3    0.0625°C    250 ms default resolution
      // wake up MCP9808 - power consumption ~200 mikro Ampere
    
      // printf("(0x%02X) Temp: %.1f *C\n", baseAddress+ad, tempFloat); 
      // set index for working sensor
      sensorIndex = (0x01 << addr) | sensorIndex;
    }
  }
  // printf("sensorByte: 0x%02X\n", sensorIndex);
}


/************************************************
 * MiniPill LoRa v2.x mapping - LoRa module RFM95W 
 ************************************************/
/* 
  PA4  // SPI1_NSS   NSS  - RFM95W
  PA5  // SPI1_SCK   SCK  - RFM95W
  PA6  // SPI1_MISO  MISO - RFM95W
  PA7  // SPI1_MOSI  MOSI - RFM95W

  PA10 // USART1_RX  DIO0 - RFM95W
  PB4  //            DIO1 - RFM95W
  PB5  //            DIO2 - RFM95W

  PA9  // USART1_TX  RST  - RFM95W

  PB6  I2C - SCL - MCP9808
  PB7  I2C - SDA - MCP9808

  VCC - 3V3
  GND - GND
*/

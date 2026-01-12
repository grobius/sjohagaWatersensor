/**
 * Water sensor with DS18B20 temperature sensor
 * Author: Johan Sjöhage
 * Based on: Leo Korbee's PSLL library examples
 *
 * Using updated PSLL library v2.1 from https://gitlab.com/iot-lab-org/stm32_lora_psll
 *
 * Hardware: MiniPill LoRa (STM32L051C8T6 with RFM95W)
 * Sensor: DS18B20 temperature sensor via OneWire
 *
 * Features:
 * - Low power operation with CLASS_A mode
 * - OTAA joining with automatic RTC calibration during join
 * - Sends temperature + VCC readings
 * - Configurable sleep interval via downlink
 * - Deep sleep between transmissions
 *
 * Downlink Protocol (Port 1):
 * Byte 0: Command
 *   0x01 = Set sleep interval
 * Byte 1-4: Sleep interval in seconds (uint32_t, big-endian)
 *
 * Example: 01 00 00 02 58 = Set sleep interval to 600 seconds (10 minutes)
 */
#include "lorawan.h"
// include for security credentials
#include "config.h"

// include for internal voltage reference
#include "STM32IntRef.h"

// EEPROM emulation for STM32
#include <EEPROM.h>

// ds18b20 libraries
#include <OneWire.h>
#include <DallasTemperature.h>

// DS18B20 pin definitions
#define ONE_WIRE_BUS PA1        // Data line (DQ)
#define DS18B20_POWER_PIN PA3   // GPIO to power the sensor (VDD) - changed from PA0 to avoid conflict with library ANALOGINPUTFORRANDOMIZING

// Setup a oneWire instance to communicate with any OneWire devices (not just Maxim/Dallas temperature ICs)
OneWire oneWire(ONE_WIRE_BUS);

// Pass our oneWire reference to Dallas Temperature.
DallasTemperature sensors(&oneWire);

// arrays to hold device address
DeviceAddress insideThermometer;

// uncomment for debugging main
#define DEBUG_MAIN

// Define constants for magic numbers
#define BAUD_RATE 115200
#define RETRY_DELAY_MS 15000
#define DEFAULT_SLEEP_INTERVAL_SECONDS 600  // 10 minutes default
#define TEMP_OFFSET 50

// EEPROM addresses for persistent storage
#define EEPROM_MAGIC_ADDR 0
#define EEPROM_SLEEP_INTERVAL_ADDR 4
#define EEPROM_MAGIC_VALUE 0xA5B6C7D8  // Magic number to validate EEPROM data

// Downlink command codes
#define CMD_SET_SLEEP_INTERVAL 0x01

#ifdef DEBUG_MAIN
  // for debugging redirect to hardware Serial2
  // Tx on PA2 - connect your Rx to PA2 and baudrate 115200,8N1
  #define DEBUG_INIT(...) HardwareSerial Serial2(USART2)
  #define DEBUG_BEGIN(...) Serial2.begin(115200)
  #define DEBUG_PRINT(...) Serial2.print(__VA_ARGS__)
  #define DEBUG_PRINTLN(...) Serial2.println(__VA_ARGS__)
  DEBUG_INIT();
#else
  #define DEBUG_BEGIN(...)
  #define DEBUG_PRINT(...)
  #define DEBUG_PRINTLN(...)
#endif

/* A BME280 object using SPI, chip select pin PA1 */
//BME280 bme(SPI,PA1);

// Sleep interval in seconds (configurable via downlink)
uint32_t sleepIntervalSeconds = DEFAULT_SLEEP_INTERVAL_SECONDS;

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

/**
 * Load sleep interval from EEPROM
 */
void loadSleepInterval() {
  uint32_t magic;
  EEPROM.get(EEPROM_MAGIC_ADDR, magic);

  if (magic == EEPROM_MAGIC_VALUE) {
    // Valid EEPROM data exists
    EEPROM.get(EEPROM_SLEEP_INTERVAL_ADDR, sleepIntervalSeconds);
    DEBUG_PRINT("Loaded sleep interval from EEPROM: ");
    DEBUG_PRINT(sleepIntervalSeconds);
    DEBUG_PRINTLN(" seconds");
  } else {
    // First boot or invalid data, initialize EEPROM
    DEBUG_PRINTLN("Initializing EEPROM with default values");
    sleepIntervalSeconds = DEFAULT_SLEEP_INTERVAL_SECONDS;
    EEPROM.put(EEPROM_MAGIC_ADDR, EEPROM_MAGIC_VALUE);
    EEPROM.put(EEPROM_SLEEP_INTERVAL_ADDR, sleepIntervalSeconds);
  }
}

/**
 * Save sleep interval to EEPROM
 */
void saveSleepInterval() {
  EEPROM.put(EEPROM_SLEEP_INTERVAL_ADDR, sleepIntervalSeconds);
  DEBUG_PRINT("Saved sleep interval to EEPROM: ");
  DEBUG_PRINT(sleepIntervalSeconds);
  DEBUG_PRINTLN(" seconds");
}

/**
 * Process downlink message
 */
void processDownlink(char *data, uint8_t length) {
  if (length < 1) return;

  uint8_t command = data[0];
  DEBUG_PRINT("Received command: 0x");
  DEBUG_PRINTLN(command, HEX);

  switch (command) {
    case CMD_SET_SLEEP_INTERVAL:
      if (length >= 5) {
        // Extract 32-bit sleep interval (big-endian)
        uint32_t newInterval = ((uint32_t)data[1] << 24) |
                               ((uint32_t)data[2] << 16) |
                               ((uint32_t)data[3] << 8) |
                               ((uint32_t)data[4]);

        // Validate range: 60 seconds to 24 hours
        if (newInterval >= 60 && newInterval <= 86400) {
          sleepIntervalSeconds = newInterval;
          saveSleepInterval();
          DEBUG_PRINT("Sleep interval updated to: ");
          DEBUG_PRINT(sleepIntervalSeconds);
          DEBUG_PRINTLN(" seconds");
        } else {
          DEBUG_PRINT("Invalid sleep interval: ");
          DEBUG_PRINTLN(newInterval);
        }
      }
      break;

    default:
      DEBUG_PRINTLN("Unknown command");
      break;
  }
}

void setup()
{
  // Setup serial debug
  DEBUG_BEGIN();
  // time for serial settings
  delay(500);
  DEBUG_PRINTLN("Setup starting");

 /************************************************
 * Load configuration from EEPROM
 ************************************************/
  loadSleepInterval();

 /************************************************
 * LoRa init and set default parameters
 ************************************************/
  if(!lora.init())
  {
    DEBUG_PRINTLN("RFM95 not detected");
    delay(5000);
    return;
  }

  // Set LoRaWAN Class change CLASS_A (needed for downlinks), CLASS_A_NO_RX or CLASS_C
  lora.setDeviceClass(CLASS_A);
  // Set Data Rate for join!
  lora.setDataRate(SF7BW125);
  // Set Receive Delay for RX1 window
  lora.setReceiveDelay1(5);
  // set channel to random
  lora.setChannel(MULTI);
  // use this setting for power settings
  // power level between 0 and max 15 (0dBm -> 15 dBm)
  lora.setTxPower1(TX_0dBm);
  // Put OTAA Key and DevAddress here
  lora.setDevEUI(devEui);
  lora.setAppEUI(appEui);
  lora.setAppKey(appKey);

 /************************************************
 * LoRa Start Join Process - calibration happens during join
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
    LowPowerCal.begin();
    LowPowerCal.deepSleep(RETRY_DELAY_MS, 0);

  }while(!isJoined);
  DEBUG_PRINTLN("Joined to network");

  DEBUG_PRINT("Time Correction: ");
  DEBUG_PRINTLN(LowPowerCal.getTimeCorrectionFactor(), 5);

 /************************************************
 * Setup DS18B20 with GPIO power control
 ************************************************/
  // Configure DS18B20 power pin
  pinMode(DS18B20_POWER_PIN, OUTPUT);
  digitalWrite(DS18B20_POWER_PIN, LOW);  // Start with sensor OFF

  // Power on sensor to initialize
  digitalWrite(DS18B20_POWER_PIN, HIGH);
  delay(50);  // Wait for sensor to stabilize

  // Start up the ds18b20 library
  sensors.begin();
  sensors.setResolution(10);  // 10-bit resolution = 0.25°C precision
  delay(100);  // Wait for configuration

  // Power off sensor until needed
  digitalWrite(DS18B20_POWER_PIN, LOW);
  DEBUG_PRINTLN("DS18B20 initialized with GPIO power control");

}

void loop()
{
 /************************************************
 * Set data ready for sending with LoRa
 * Payload format:
 * Byte 0-1: VCC (mV)
 * Byte 2-3: Temperature (°C × 100 + 5000)
 * Byte 4-7: Current sleep interval (seconds)
 ************************************************/
  uint8_t dataLength = 8;
  char data[dataLength];

  int dataIndex = 0;

  // read vcc and add to bytebuffer
  int32_t vcc = IntRef.readVref();
  data[dataIndex++] = (vcc >> 8) & 0xff;
  data[dataIndex++] = (vcc & 0xff);

  // Power on DS18B20 sensor
  digitalWrite(DS18B20_POWER_PIN, HIGH);
  delay(50);  // Wait for sensor to stabilize

  // reading data from ds18b20
  sensors.requestTemperatures();
  float tempFloat = sensors.getTempCByIndex(0);

  // Power off DS18B20 to save energy
  digitalWrite(DS18B20_POWER_PIN, LOW);

  if (tempFloat == DEVICE_DISCONNECTED_C)
  {
    DEBUG_PRINTLN("Error: Could not read temperature data");
    tempFloat = 999;
    return;
  }
  tempFloat = tempFloat + TEMP_OFFSET;    //Using 50 degree offset to handle negative values. Should be subtracted in payload decoder

  // from float to uint16_t
  uint16_t tempInt = (100 * tempFloat);

  // move temperature data into bytebuffer
  data[dataIndex++] = (tempInt >> 8) & 0xff;
  data[dataIndex++] = tempInt & 0xff;

  // add current sleep interval (32-bit, big-endian)
  data[dataIndex++] = (sleepIntervalSeconds >> 24) & 0xff;
  data[dataIndex++] = (sleepIntervalSeconds >> 16) & 0xff;
  data[dataIndex++] = (sleepIntervalSeconds >> 8) & 0xff;
  data[dataIndex++] = sleepIntervalSeconds & 0xff;

  DEBUG_PRINT("Frame Counter: ");
  DEBUG_PRINTLN(lora.getFrameCounter());
  DEBUG_PRINT("Sleep Interval: ");
  DEBUG_PRINT(sleepIntervalSeconds);
  DEBUG_PRINTLN(" seconds");

  // unconfirmed data (0), port (1)
  lora.setUplinkData(data, dataIndex, 0, 1);

 /************************************************
 * LoRa Send data and wait for received data
 ************************************************/
  // cycle -> send (if new message available) and wait for response in RX1 and RX2
  lora.cycle();

 /************************************************
 * Check if data is received and print
 ************************************************/
  recvStatus = lora.readData(outStr);
  if(recvStatus)
  {
    DEBUG_PRINT("Received downlink: ");
    int i = 0;
    while(outStr[i] != '\0' && i < sizeof(outStr))
    {
        DEBUG_PRINT(outStr[i], HEX);
        DEBUG_PRINT(" ");
        i++;
    }
    DEBUG_PRINTLN();

    // Process downlink command
    processDownlink(outStr, recvStatus);
  }

 /************************************************
 * Power down and going to long sleep
 ************************************************/
  DEBUG_PRINT("Going to sleep for ");
  DEBUG_PRINT(sleepIntervalSeconds);
  DEBUG_PRINTLN(" seconds");

  lora.sleepLowPower();
  LowPowerCal.begin();
  LowPowerCal.deepSleep(sleepIntervalSeconds * 1000, 0);  // Convert to milliseconds
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

/* MiniPill LoRa pin mapping - RFM95W LoRa module and DS18B20 sensor

  RFM95W LoRa Module:
  PA4  // SPI1_NSS   NSS  - RFM95W
  PA5  // SPI1_SCK   SCK  - RFM95W
  PA6  // SPI1_MISO  MISO - RFM95W
  PA7  // SPI1_MOSI  MOSI - RFM95W
  PA9  // USART1_TX  RST  - RFM95W
  PA10 // USART1_RX  DIO0 - RFM95W
  PB4  //            DIO1 - RFM95W
  PB5  //            DIO2 - RFM95W

  DS18B20 Temperature Sensor (GPIO-controlled power):
  PA3  //            VDD  - DS18B20 (power control) - changed from PA0 to avoid conflict with ANALOGINPUTFORRANDOMIZING
  PA1  //            DQ   - DS18B20 (data with 4.7kΩ pullup to PA3)
  GND  //            GND  - DS18B20

  Debug Serial (optional):
  PA2  // USART2_TX  TX   - Debug output (115200 baud)

  Power:
  VCC - 3V3
  GND - GND
*/

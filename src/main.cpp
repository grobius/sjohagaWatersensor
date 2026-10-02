/**
 * MiniPill LoRa water temperature sensor (STM32L051C8T6 / RFM95W).
 * Based on Leo Korbee's OTAA example and calibrated low-power libraries.
 */
#include "lorawan.h" // Includes PSLL's STM32LowPowerCal wrapper.
#include "config.h"
#include "STM32IntRef.h"
#include "ReadingInterval.h"
#include <OneWire.h>
#include <DallasTemperature.h>
#include <math.h>

// DS18B20 data on PA1; SPI uses PA5/PA6/PA7.
OneWire oneWire(PA1);
DallasTemperature sensors(&oneWire);

// Uncomment for serial diagnostics (RX PA3, TX PA2).
//#define DEBUG_MAIN
#ifdef DEBUG_MAIN
HardwareSerial Serial2(PA3, PA2);
#define APP_DEBUG_INIT() Serial2.begin(9600)
#define APP_DEBUG_PRINTLN(...) Serial2.println(__VA_ARGS__)
#define APP_DEBUG_FLUSH() Serial2.flush()
#else
#define APP_DEBUG_INIT()
#define APP_DEBUG_PRINTLN(...)
#define APP_DEBUG_FLUSH()
#endif

constexpr uint32_t INITIAL_JOIN_RETRY_MS = 15000;
constexpr uint32_t MAX_JOIN_RETRY_MS = 3600000;
constexpr float TEMP_OFFSET = 50.0f;
constexpr uint16_t INVALID_TEMPERATURE = 0xFFFF;
uint32_t readingIntervalSeconds = ReadingInterval::DEFAULT_SECONDS;

const sRFM_pins RFM_pins = {
    .MOSI = PA7,
    .CS = PA4,
    .RST = PA9,
    .DIO0 = PA10,
    .DIO1 = PB4,
    .DIO2 = PB5
};

void setup() {
    APP_DEBUG_INIT();
    APP_DEBUG_PRINTLN("Water sensor boot; calibrating RTC for 8 seconds");
    APP_DEBUG_PRINTLN("Configured DevEUI:");
    APP_DEBUG_PRINTLN(devEui);
    LowPower.begin();
    // Measure the actual sleep clock against SysTick, also allowing time to
    // attach a programmer. Join timing alone cannot measure RTC drift.
    if (!LowPowerCal.calibrateRTC()) {
        APP_DEBUG_PRINTLN("RTC calibration failed; using correction factor 1");
    }
    APP_DEBUG_PRINTLN("RTC correction factor:");
    APP_DEBUG_PRINTLN(LowPowerCal.getTimeCorrectionFactor(), 5);
    APP_DEBUG_PRINTLN("Initializing RFM95");

    // Never enter loop() with an uninitialized radio.
    while (!lora.init()) {
        APP_DEBUG_PRINTLN("RFM95 not detected; retrying in 60 seconds");
        APP_DEBUG_FLUSH();
        LowPowerCal.deepSleep(60000, 0);
    }
    APP_DEBUG_PRINTLN("RFM95 detected");

    lora.setDeviceClass(CLASS_A);
    lora.setDataRate(SF7BW125);
    lora.setChannel(MULTI);
    // Preserve the intended low-power PA_BOOST setting and disable the
    // high-power PA_DAC mode left by RFM_Init().
    lora.setTxPower(2, PA_BOOST_PIN);
    lora.setDevEUI(devEui);
    lora.setAppEUI(appEui);
    lora.setAppKey(appKey);

    // Back off during network outages instead of attempting a join every 15 s.
    uint32_t retryMs = INITIAL_JOIN_RETRY_MS;
    while (true) {
        APP_DEBUG_PRINTLN("Sending OTAA join request");
        APP_DEBUG_FLUSH();
        if (lora.join()) break;
        lora.sleepLowPower();
        APP_DEBUG_PRINTLN("Join failed; sleeping before retry");
        APP_DEBUG_FLUSH();
        const uint32_t jitterMs = uint32_t(random(0, retryMs / 4 + 1));
        LowPowerCal.deepSleep(retryMs + jitterMs, 0);
        retryMs = (retryMs < MAX_JOIN_RETRY_MS / 2) ? retryMs * 2 : MAX_JOIN_RETRY_MS;
    }
    APP_DEBUG_PRINTLN("Joined to network");
    lora.sleepLowPower();

    sensors.begin();
    sensors.setResolution(10);
}

void loop() {
    char data[4];
    const int32_t vcc = IntRef.readVref();
    data[0] = (vcc >> 8) & 0xFF;
    data[1] = vcc & 0xFF;

    sensors.requestTemperatures();
    const float temperature = sensors.getTempCByIndex(0);
    uint16_t encodedTemperature = INVALID_TEMPERATURE;
    // The existing unsigned offset encoding supports -50 through +125 C.
    if (isfinite(temperature) && temperature >= -50.0f && temperature <= 125.0f) {
        encodedTemperature = uint16_t(lroundf((temperature + TEMP_OFFSET) * 100.0f));
    } else {
        APP_DEBUG_PRINTLN("Temperature unavailable");
        // Rediscover a sensor plugged in after boot or a temporary disconnect.
        sensors.begin();
        sensors.setResolution(10);
    }
    data[2] = (encodedTemperature >> 8) & 0xFF;
    data[3] = encodedTemperature & 0xFF;

    // Binary payload: sizeof, never strlen. Send even on sensor failure so that
    // the node stays reachable and can receive configuration downlinks.
    lora.setUplinkData(data, sizeof(data), 0, 1);
    lora.cycle();

    uint8_t downlink[MAX_DOWNLINK_PAYLOAD_SIZE];
    uint8_t port = 0;
    const int length = lora.readData(downlink, sizeof(downlink), port);
    if (length > 0 && ReadingInterval::apply(port, downlink, size_t(length), readingIntervalSeconds)) {
        APP_DEBUG_PRINTLN("Reading interval updated (seconds):");
        APP_DEBUG_PRINTLN(readingIntervalSeconds);
    }

    lora.sleepLowPower();
    APP_DEBUG_FLUSH();
    // STM32LowPower takes milliseconds. Measurement and radio time are additional.
    LowPowerCal.deepSleep(uint32_t(readingIntervalSeconds * 1000UL), 0);
}

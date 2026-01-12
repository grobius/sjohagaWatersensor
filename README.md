# sjohagaWatersensor

Low power LoRa node for DS18B20 temperature sensor with remote configuration

## Overview

This project implements a low-power LoRaWAN water temperature sensor using:
- **Hardware**: STM32L051C8T6 (MiniPill LoRa board) with RFM95W LoRa module
- **Sensor**: DS18B20 temperature sensor via OneWire protocol
- **Network**: LoRaWAN with OTAA (Over-The-Air Activation)

## Features

- **Ultra-low power operation**: Uses CLASS_A mode with configurable sleep intervals
- **Remote configuration**: Set sleep interval via LoRaWAN downlink messages
- **Automatic RTC calibration**: Time calibration happens during the join procedure
- **Temperature + VCC + Config monitoring**: Sends sensor temperature, supply voltage, and current sleep interval
- **Persistent storage**: Sleep interval stored in EEPROM, survives reboots
- **Deep sleep**: Extended sleep periods between transmissions (60 seconds to 24 hours)
- **PSLL v2.1**: Using the latest Plain and Simple LoRaWAN Library

## Library Update (2026-01-11)

This project has been **updated to use the latest PSLL v2.1** library from:
- **Repository**: https://gitlab.com/iot-lab-org/stm32_lora_psll
- **Blog post**: https://www.iot-lab.org/blog/1209/

### Key Improvements in PSLL v2.1

1. **Better low-power operation**: Achieves up to 1.7 µA in sleep mode
2. **More robust join procedure**: No power down during join, uses RSSI-based randomization
3. **Automatic clock calibration**: Uses the 5-second join delay as timing reference
4. **SPI power management**: Proper SPI.end()/SPI.begin() for low power
5. **32-bit frame counter**: Support for longer operation without rollover
6. **Compatible with latest STM32 framework**: Works with PlatformIO STM32 v15.4.1+

## Hardware Configuration

### Pin Mapping (MiniPill LoRa)

```
RFM95W Module:
  PA4  - NSS (CS)
  PA5  - SCK
  PA6  - MISO
  PA7  - MOSI
  PA9  - RST
  PA10 - DIO0
  PB4  - DIO1
  PB5  - DIO2

DS18B20 Sensor (GPIO-controlled power):
  PA3  - VDD (power control via GPIO)
  PA1  - DQ (data line with 4.7kΩ pullup to PA3)
  GND  - GND

Debug Serial (optional):
  PA2  - TX (115200 baud)
```

**Important**: The DS18B20 uses GPIO-controlled power (PA3) for ultra-low power operation. The 4.7kΩ pullup resistor should be connected between PA1 (data) and PA3 (power), NOT to 3.3V. This ensures zero power consumption when the sensor is off.

**Note**: PA3 was chosen instead of PA0 to avoid conflict with the PSLL library's analog randomization routine which uses PA0 for DevNonce generation.

### DS18B20 Wiring Diagram

```
STM32L051                          DS18B20
                                   (TO-92)
   PA3 (GPIO) ────┬──────────────── Pin 3 (VDD)
                  │
                4.7kΩ (pullup)
                  │
   PA1 (Data) ────┴──────────────── Pin 2 (DQ)

   GND ─────────────────────────── Pin 1 (GND)
```

**Why GPIO-controlled power?**
- **Zero power when sleeping**: PA3 LOW = 0 µA sensor consumption
- **99% power savings**: Compared to always-on or parasitic power
- **Better reliability**: Normal power mode vs parasitic mode
- **Faster readings**: No capacitor charging delays needed

**Alternative wiring (always-on, not recommended):**
If you want to use always-on power instead (wastes ~1 µA continuously):
- Connect DS18B20 VDD to 3.3V (not PA3)
- Connect 4.7kΩ pullup between PA1 and 3.3V
- Modify code to remove `digitalWrite(DS18B20_POWER_PIN, ...)` calls

## Configuration

1. Copy `src/config_example.h` to `src/config.h`
2. Fill in your LoRaWAN credentials:
   ```cpp
   const char *devEui = "YOUR_DEV_EUI";
   const char *appEui = "YOUR_APP_EUI";
   const char *appKey = "YOUR_APP_KEY";
   ```
3. Default sleep interval is 600 seconds (10 minutes) - can be changed via downlink
4. Uncomment `#define DEBUG_MAIN` in [main.cpp](src/main.cpp) for serial debugging

## Dependencies

- **OneWire** (v2.3.8+): For DS18B20 communication
- **DallasTemperature** (v3.11.0+): Temperature sensor library
- **EEPROM** (v2.0.1+): For persistent configuration storage
- **PSLL v2.1**: Included in `lib/PSLL/`
- **STM32LowPower**: Provided by STM32 framework
- **STM32RTC**: Provided by STM32 framework
- **STM32IntRef**: Included in `lib/STM32IntRef/`

## Building

```bash
# Using PlatformIO
pio run

# Upload to board
pio run --target upload
```

## LoRaWAN Communication

### Uplink Payload Format (Port 1)

The sensor transmits 8 bytes per message:

| Byte 0-1 | Byte 2-3 | Byte 4-7 |
|----------|----------|----------|
| VCC (mV) | Temperature (°C × 100 + 5000) | Sleep Interval (seconds) |

**Note**: Temperature uses a 50°C offset to handle negative values. Subtract 5000 from the raw value and divide by 100 to get actual temperature.

#### Example Uplink Decoder (TTN/ChirpStack)

```javascript
function decodeUplink(input) {
  var vcc = (input.bytes[0] << 8) | input.bytes[1];
  var tempRaw = (input.bytes[2] << 8) | input.bytes[3];
  var temperature = (tempRaw / 100) - 50;
  var sleepInterval = (input.bytes[4] << 24) |
                      (input.bytes[5] << 16) |
                      (input.bytes[6] << 8) |
                      input.bytes[7];

  return {
    data: {
      vcc: vcc,
      temperature: temperature,
      sleepIntervalSeconds: sleepInterval
    }
  };
}
```

### Downlink Payload Format (Port 1)

Send commands to configure the device remotely.

#### Set Sleep Interval

**Command**: `0x01`
**Format**: `01 [4 bytes: interval in seconds, big-endian]`
**Valid range**: 60 to 86400 seconds (1 minute to 24 hours)

**Examples**:

| Description | Hex Payload | Explanation |
|-------------|-------------|-------------|
| 5 minutes | `01 00 00 01 2C` | 01 (command) + 300 seconds |
| 10 minutes | `01 00 00 02 58` | 01 (command) + 600 seconds |
| 1 hour | `01 00 00 0E 10` | 01 (command) + 3600 seconds |
| 6 hours | `01 00 00 54 60` | 01 (command) + 21600 seconds |

**Response**: The device will acknowledge the new setting in the next uplink by reporting the updated sleep interval in bytes 4-7.

#### Downlink Example (TTN/ChirpStack)

```javascript
// Set sleep interval to 1 hour (3600 seconds)
function encodeDownlink(input) {
  var seconds = input.data.sleepIntervalSeconds || 3600;

  return {
    bytes: [
      0x01,  // Command: Set sleep interval
      (seconds >> 24) & 0xFF,
      (seconds >> 16) & 0xFF,
      (seconds >> 8) & 0xFF,
      seconds & 0xFF
    ],
    fPort: 1
  };
}
```

## Power Consumption

With CLASS_A mode, GPIO-controlled sensor power, and proper deep sleep:
- **Deep sleep**: ~1.7 µA (MCU + RFM95W)
- **Sensor power consumption**: ~0 µA (GPIO OFF during sleep)
- **Active (measuring)**: ~1 mA for ~750ms (sensor reading)
- **Active (transmitting)**: ~20-40 mA for <2 seconds
- **Receive windows**: ~20 mA for <100ms per transmission
- **Total average**: Depends on sleep interval (see table below)

### Estimated Battery Life

With a 2000 mAh battery and GPIO-controlled sensor power:

| Sleep Interval | Messages/Day | Avg Current | Estimated Life |
|---------------|--------------|-------------|----------------|
| 5 minutes | 288 | ~15 µA | ~6 months |
| 10 minutes | 144 | ~8 µA | ~12 months |
| 30 minutes | 48 | ~4 µA | ~24 months |
| 1 hour | 24 | ~3 µA | ~30+ months |
| 6 hours | 4 | ~2 µA | ~5+ years |

**Power Budget Calculation (10-minute interval example):**
- Sleep: 600s × 1.7 µA = 0.28 µAh
- Sensor reading: 0.75s × 1 mA = 0.21 µAh
- Transmission: 2s × 30 mA = 16.7 µAh
- **Total per cycle**: ~17 µAh
- **Average current**: 17 µAh / (600s / 3600) = ~10 µA
- **Battery life**: 2000 mAh / 0.01 mA = ~200,000 hours = ~23 years (theoretical)

*Note: Actual battery life depends on temperature, battery self-discharge (~2% per month), and network conditions. Practical life is typically 30-50% of theoretical due to battery aging.*

### Power Comparison: GPIO vs Always-On

| Power Method | Sleep Current | Annual Consumption |
|--------------|---------------|-------------------|
| **GPIO-controlled** (recommended) | **1.7 µA** | **~15 mAh** |
| Always-on 3.3V | ~2-3 µA | ~20-25 mAh |
| Parasitic power | ~700 µA | ~6,000 mAh |

The GPIO-controlled method achieves **99.75% power savings** compared to parasitic power!

## EEPROM Storage

The device stores configuration in EEPROM to survive power cycles:
- **Address 0-3**: Magic number (0xA5B6C7D8) for validation
- **Address 4-7**: Sleep interval in seconds (uint32_t)

On first boot, default values are written to EEPROM.

## Migration from Old PSLL Version

The old library has been backed up to `lib/PSLL_old_backup_20260111/`. Key changes:

1. **CLASS_A mode enabled**: Now supports downlink messages (was CLASS_A_NO_RX)
2. **No manual calibration needed**: RTC calibration happens automatically during join
3. **Updated power management**: Use `LowPowerCal.begin()` and `LowPowerCal.deepSleep(ms, 0)`
4. **Better debug macros**: `DEBUG_BEGIN()` instead of `DEBUG_INIT()`
5. **32-bit frame counter**: Prevents rollover issues for long-term deployments
6. **Remote configuration**: Sleep interval can be changed via downlink

## Troubleshooting

### Device not joining
- Check LoRaWAN credentials in `config.h`
- Verify antenna connection
- Check gateway is in range and operational
- Enable DEBUG_MAIN for diagnostic output from main application
- Enable DEBUG_LORA in `lib/PSLL/src/LoRaWAN/Config.h` for detailed library diagnostics (DevNonce, join request/accept, MIC validation)
- **Important**: Ensure PA0 is not used for other purposes - the library uses it for analog randomization
- Check network server console to see if join requests are being received

### Downlinks not received
- Ensure device uses CLASS_A mode (not CLASS_A_NO_RX)
- Check downlink is sent to correct port (Port 1)
- Verify downlink timing (must be sent during RX windows)
- Some networks require uplink first to trigger downlink

### Sleep interval not updating
- Check downlink payload format
- Verify value is within valid range (60-86400 seconds)
- Next uplink will report current sleep interval
- Enable DEBUG_MAIN to see processing messages

### Sensor reading errors (999 or DEVICE_DISCONNECTED)
- Verify DS18B20 wiring (see wiring diagram above)
- Check 4.7kΩ pullup resistor is connected between PA1 and PA3 (NOT 3.3V)
- Ensure PA3 is configured as output and toggling (enable DEBUG_MAIN)
- Try increasing delay after `digitalWrite(DS18B20_POWER_PIN, HIGH)` to 100ms
- Test with always-on power (connect VDD to 3.3V) to isolate GPIO issue
- Verify sensor is genuine DS18B20 (many clones have timing issues)

## License

Based on the PSLL library which uses MIT License. Refer to individual source files for details.

## Credits

- **PSLL Library**: Leo Korbee - https://gitlab.com/iot-lab-org/stm32_lora_psll
- **Original work**: Gerben den Hartog (Ideetron), Andri Rahmadhani, Beelan team
- **Hardware**: MiniPill LoRa design by Leo Korbee

## References

- [PSLL Library GitLab](https://gitlab.com/iot-lab-org/stm32_lora_psll)
- [Blog Post: STM32 Plain and Simple LoRaWAN Library](https://www.iot-lab.org/blog/1209/)
- [LoRaWAN 1.0.3 Specification](https://lora-alliance.org/resource_hub/lorawan-specification-v1-0-3/)
- [The Things Network](https://www.thethingsnetwork.org/)

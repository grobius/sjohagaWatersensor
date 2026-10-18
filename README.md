# sjohagaWatersensor

Battery-powered DS18B20 water temperature sensor using a MiniPill LoRa
(STM32L051C8T6 + RFM95W), EU868 and LoRaWAN OTAA, Class A.

Each cycle the device reads the supply voltage and water temperature, sends one
uplink, listens in RX1/RX2 for a downlink and deep-sleeps for the reading
interval (10 minutes by default).

## Build and flash

```sh
pio run -e genericL051C            # build
pio run -e genericL051C -t upload  # flash via ST-Link
```

The firmware contains no credentials; the same image runs on every device.

## Provision a device

Each device's DevEUI, AppEUI/JoinEUI and AppKey are stored in the STM32L0 data
EEPROM at `0x08080000`, which firmware uploads do not erase. Provision a device
once, before or after flashing the firmware:

1. Copy `devices_example.csv` to `devices.csv` (ignored by Git) and add one row
   per device with the values from the TTN console.
2. Connect the ST-Link and run `python tools/provision.py <name>`.
   `python tools/provision.py --list` shows the available names.

The script writes a 76-byte record (magic, version, ASCII-hex keys, CRC-32; see
`include/DeviceIdentity.h`) to `.provision/<name>.bin` and programs it with
STM32CubeProgrammer, connecting under reset so that a sleeping MCU is reachable.
Use `--mode HotPlug` if NRST is not wired to the programmer (attach during the
eight-second calibration after reset), and `--no-flash` to only build the image.

At boot the firmware validates the record. If it is blank or corrupt, the device
does not try to join; it sleeps and checks again every hour. Back up `devices.csv`
outside the repository; it is the only copy of the keys besides the network console.

## Wiring the DS18B20

Use the probe's three wires (normal power, not parasite power). The firmware
powers the probe from **PB11** only while reading, and reads data on **PB10**:

```
 MiniPill                                   DS18B20 probe
 PB11 (sensor power) ─┬──────────────────── VDD  (usually red)
                      │
                   [4.7 kΩ]  pull-up
                      │
 PB10 (1-Wire data) ──┴──────────────────── DQ   (usually yellow or white)

 GND ────────────────────────────────────── GND  (usually black)
```

- Connect the pull-up to **PB11**, not 3V3; otherwise the switched-off probe is
  powered through its data pin. PB10 and PB11 are adjacent, so the resistor can
  be soldered across their pads at the board end.
- Use 4.7 kΩ for cables up to a few metres, 2.2–3.3 kΩ for longer cables or if
  readings drop out. Optionally fit 100 nF between VDD and GND at the probe.
- Wire colours vary between manufacturers; check the probe's documentation.
- Do not use PA13/PA14 (SWD), PA2/PA3 (debug serial) or the radio pins PA4–PA7,
  PA9, PA10, PB4 and PB5. To use other GPIOs, update `oneWire(...)` and
  `SENSOR_POWER_PIN` in `src/main.cpp`.

For each reading the firmware drives PB11 high, waits 10 ms, searches the bus,
sets 10-bit resolution, converts (up to 187.5 ms) and switches PB11 low again.
Many DS18B20 clones return 85 °C (the power-on value) from the first conversion
after power-up, so an 85 °C result is converted once more; a second 85 °C is
reported as a failed reading.

The DS18B20 needs at least 3.0 V and the GPIO drops about 0.1 V, so with a
battery feeding the MCU directly, readings may fail (`FFFF`) once VCC falls to
about 3.1 V. To power the probe permanently instead, connect VDD and the pull-up
to **3V3** and comment out `#define SENSOR_POWER_PIN PB11`.

## Uplink format

FPort **1**, four bytes, most significant byte first:

| Bytes | Meaning |
| --- | --- |
| 0–1 | MCU supply voltage in millivolts |
| 2–3 | `(temperature_C + 50) * 100`, or `FFFF` if the reading failed |

Decode temperature as `raw / 100 - 50` (range −50 to +125 °C). With a regulator,
the voltage is MCU VDD, not the battery voltage. Use
[tools/ttn-uplink-decoder.js](tools/ttn-uplink-decoder.js) as the uplink
formatter in The Things Stack; it returns `vcc` in volts, `temperature` in °C,
and `temperature: null, sensor_ok: false` for `FFFF`. The device keeps sending
uplinks and accepting downlinks when the sensor fails.

## Set the reading interval by downlink

Queue a **binary downlink on FPort 2** with five bytes: `01` followed by the
interval in seconds as an unsigned 32-bit big-endian value.

| Interval | Hex payload |
| --- | --- |
| 1 minute | `010000003C` |
| 10 minutes (default) | `0100000258` |
| 30 minutes | `0100000708` |
| 1 hour | `0100000E10` |
| 24 hours | `0100015180` |

- Accepted range: **60–86400 seconds**. Anything else is ignored.
- Class A: the downlink is delivered after the device's next uplink, so it can
  take up to one current interval to apply.
- The interval is the sleep time; conversion and radio time come on top.
- The setting is held in RAM and returns to **600 seconds after a reset**.
- There is no confirmation uplink; check the timing of the following uplinks or
  the debug log (`Reading interval updated`).

## Debugging

Uncomment `#define DEBUG_MAIN` in `src/main.cpp`, rebuild and flash. The log is on
PA2 (TX) / PA3 (RX) at 9600 baud and shows the RTC correction factor, the DevEUI
read from EEPROM, radio initialization, join attempts, sensor failures and
interval changes. Keys are never printed. Comment `DEBUG_MAIN` out again before
deploying on battery.

Radio-level logging (`DEBUG_LORA`, `DEBUG_LOW_LEVEL` in
`lib/PSLL/src/LoRaWAN/Config.h`) also requires `DEBUG_MAIN`.

If no join requests appear, check that the device was provisioned with the row
matching the device in TTN, and look at the gateway's live data. Failed joins back
off from 15 seconds to one hour (plus jitter), so long quiet periods are normal.

## Timing

At boot, `LowPowerCal.calibrateRTC()` measures the internal RTC against SysTick
for eight seconds (no LSE crystal), and the resulting factor corrects all sleeps:
RX1/RX2 waits, join retries and the reading interval. RX windows are scheduled
from the end of transmission with a 20 ms early wake.

## Tests

Compile-time checks for the downlink parser, the EEPROM identity record and the
sleep timing math:

```sh
arm-none-eabi-g++ -std=c++14 -Wall -Wextra -Werror -Iinclude -fsyntax-only test/reading_interval_checks.cpp
arm-none-eabi-g++ -std=c++14 -Wall -Wextra -Werror -Iinclude -fsyntax-only test/device_identity_checks.cpp
arm-none-eabi-g++ -std=c++14 -Wall -Wextra -Werror -Ilib/PSLL/src/STM32LowPowerCal -fsyntax-only test/sleep_timing_checks.cpp
```

## Known limitations

- **LoRaWAN stack:** payload encryption uses only the low 16 bits of the frame
  counter, so it breaks after 65535 uplinks (about 455 days at 10-minute
  intervals). Downlink counters are 16-bit with no replay check, DevNonces are not
  tracked persistently, ADR and CFList are not implemented, and MAC command
  support is partial. Fix or migrate to a maintained stack before long deployments.
- **Battery life is unmeasured.** Measure sleep current with the debugger
  disconnected. The CPU stays awake during the DS18B20 conversion.
- **No watchdog.** A hang in the radio driver is limited by 15-second TX/RX
  timeouts, but there is no hardware reset recovery.
- **Credentials once committed to Git** should be rotated in the network console,
  then reprovisioned. Consider readout protection (RDP level 1) on deployed
  devices, since SWD can read the EEPROM.

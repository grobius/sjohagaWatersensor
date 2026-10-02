# sjohagaWatersensor

Battery-powered DS18B20 water temperature sensor using a MiniPill LoRa
(STM32L051C8T6 + RFM95W), EU868 and LoRaWAN OTAA.

## Build

Copy `src/config_example.h` to `src/config.h` for a new installation, enter the
OTAA credentials and set the board-specific `calTimeDivider`. Preserve an existing
configuration. Build with `pio run -e genericL051C`.

### Current build issue

The merged firmware at commit `945d2b4` fails to build with
`STM32LowPowerCal.h: No such file or directory` (checked 2026-10-02).
The PSLL update moved that header to `lib/PSLL/src/STM32LowPowerCal/`, while
`src/main.cpp` still includes it using the old path. The application also calls
`setRTCCalibrationTime()` and `calibrateRTC()`, which the new library no longer
provides. The application and library calibration APIs need to be reconciled
before building and flashing this version. The protocol below describes the
interval handler present in the source.

### Keep the local branch current

Switching to local `main` does not automatically update it from GitHub. Commit
or stash local changes first, then run:

```sh
git switch main
git pull --ff-only origin main
```

Build successfully and upload the resulting firmware to the sensor after any
firmware update. Pulling the source alone does not update the device.

## Adjust the reading interval by downlink

Queue a **binary downlink on FPort 2**, containing exactly five bytes:

| Bytes | Meaning |
| --- | --- |
| 0 | `01`: set reading interval |
| 1–4 | Unsigned 32-bit interval in **seconds**, most significant byte first |

| Interval | Hex payload |
| --- | --- |
| 1 minute | `010000003C` |
| 10 minutes (boot default) | `0100000258` |
| 30 minutes | `0100000708` |
| 1 hour | `0100000E10` |
| 24 hours | `0100015180` |

In The Things Stack, queue a downlink for this device, select FPort **2**,
and enter the hex payload. An unconfirmed downlink is sufficient; if confirmed,
its LoRaWAN acknowledgement is sent with the next scheduled uplink.

The firmware accepts **60–86400 seconds**, inclusive. Incorrect ports, command
IDs, lengths and out-of-range values leave the current setting unchanged.
The value controls sleep after each measurement/transmission cycle, so the actual
time between readings also includes sensor conversion and radio activity.
The new setting applies to the sleep immediately after reception. It is held in
RAM and returns to **600 seconds after reset or power loss**. No flash writes are
performed for configuration changes. There is no application-level response;
check the next uplink's timing or enable `DEBUG_MAIN` to see the applied setting.

The device uses **Class A**: it receives queued downlinks after an uplink, so a
sleeping device cannot be updated immediately. A longer interval also increases
configuration latency. See [The Things Stack's remote-control guide](https://www.thethingsindustries.com/docs/getting-started/3-try-starter-kit/control-remotely/).
Enabling the receive windows costs energy compared with the previous
`CLASS_A_NO_RX` configuration. Measure that cost on the board.

### If the interval does not change

- Use **FPort 2** for commands. FPort 1 carries sensor uplinks and is rejected by
  the interval handler.
- Select **Bytes** in The Things Stack and enter, for example, `01 00 00 0E 10`
  for one hour. Queue the command before the next uplink.
- Verify the flashed firmware includes `ReadingInterval.h` and uses `CLASS_A`.
  The older firmware uses `CLASS_A_NO_RX` and a fixed sleep interval.
- Allow the existing interval to elapse for delivery. After successful reception,
  the following uplink should use the new interval, plus measurement/radio time
  and RTC clock error.
- A `ns.down.data.schedule.attempt` event alone does not prove device reception.
  Inspect scheduling results, gateway events and device serial diagnostics.
- A reset restores the ten-minute default; the setting is not persistent.

## Uplink format

FPort **1**, exactly **four binary bytes**. Both fields use most significant byte first.

| Bytes | Meaning |
| --- | --- |
| 0–1 | MCU supply voltage in millivolts |
| 2–3 | `(temperature_C + 50) * 100`, or `FFFF` for an unavailable/unencodable reading |

Decode temperature as `raw / 100 - 50`; treat `FFFF` as missing data.
The existing encoding covers −50 to +125 °C. Voltage measures MCU VDD; with a
regulator, it does not directly measure the battery. Valid temperature readings
retain the existing wire format. Update a server decoder to handle the new
failure sentinel before deployment.

## Fixes included

- Replaced `strlen()` on the binary uplink with an explicit four-byte length.
  The previous code could truncate at a zero byte or read beyond the array.
- Sensor failure now sends `FFFF` and reaches sleep, keeping downlinks available.
  Previously it returned to `loop()` continuously without sleeping.
- Failed radio initialization retries after sleep instead of entering the main
  loop with an uninitialized radio. Failed joins use exponential backoff from
  15 seconds to one hour, with up to 25% additional jitter.
- Removed the 100 ms active delay before each long sleep and corrected sleep
  units in the application comments: `deepSleep()` takes milliseconds.
- Enabled Class A and added a bounded binary receive API exposing FPort.
  Corrected FPort extraction when FOpts are present, the EU868 RX2 channel,
  and use of join-accept RX delay, RX1 data-rate offset and RX2 data rate.
- Added downlink length checks and kept unauthenticated packet metadata out of
  the ACK/MAC processing path. A valid ACK-only RX1 packet now skips RX2.
- Bounded MAC option responses and corrected their lengths. This remains a
  partial MAC implementation: FOpts DevStatusReq and RXTimingSetupReq are handled;
  RXParamSetupReq is rejected, and parsing stops at other command IDs. MAC commands
  carried in FPort 0 are not implemented.

## Further battery and stability improvements

These are review findings and proposed follow-up work, not measured battery gains.

1. **Measure sleep current with the debugger disconnected.** Check the regulator,
   board LEDs, sensor supply, pull-ups and GPIO leakage. At ten-minute intervals,
   continuous leakage can dominate average current. No schematic or current
   measurements are included in this repository.
2. **Use a longer interval when acceptable.** Moving from 10 to 30 minutes reduces
   measurement/transmission cycles by approximately two-thirds, but leaves sleep
   current unchanged. Check network airtime limits before using short intervals.
3. **Sleep during DS18B20 conversion.** The current DallasTemperature call waits
   with the CPU active. At 10-bit resolution the conversion limit is 187.5 ms;
   9-bit resolution takes 93.75 ms and gives 0.5 °C steps. Use asynchronous
   conversion and a verified wake timer if the application tolerates it. Confirm
   whether the probe uses an external supply or parasite power before changing
   pull-up/power behavior. See [Analog Devices' resolution comparison](https://www.analog.com/en/resources/design-notes/1wire-protocol-pdf-of-ds18s20-vs-ds18b20-digital-themormeters.html).
4. **Add radio timeouts and recovery.** `RFM95.cpp` waits indefinitely for DIO
   transitions during TX and RX. A disconnected or stuck radio can leave the CPU
   awake forever. Bound these waits, propagate failures and reset/reinitialize
   the radio. A watchdog is useful only with a plan for feeding it during the
   allowed long sleeps; enabling one blindly can cause repeated resets/joins.
5. **Repair or replace the incomplete LoRaWAN stack before long deployments.**
   The merged library increments a 32-bit TX counter and includes its upper bytes
   in the MIC, but `Encrypt_Payload()` still sets the upper counter bytes to zero.
   Payload encryption therefore needs correction when the counter passes 65535
   (roughly 455 days at ten-minute intervals, excluding active time). Received
   counters are reconstructed from only 16 bits, and downlink replay checks are
   absent. DevNonce generation has no persistent uniqueness tracking. Complete
   counter handling, replay rejection and durable nonce allocation, or migrate
   to a maintained stack.
6. **Verify receive timing and network compatibility on hardware.** The library
   uses RTC timing corrections, ignores CFList channel updates and does not
   implement ADR. Verify join-accept handling in both receive windows.
   Test RX1/RX2 across temperature and supply voltage; consider an LSE crystal
   if the board supports it. Keep the network channel plan compatible with the
   library's fixed channel table. SF7 and the current TX-power setting need an
   adequate link margin at the actual installation.
   Also review the power helper: initialization enables high-power PA_DAC,
   while `setTxPower1(0)` only changes PA_CONFIG. The existing
   `setTxPower(dBm, PA_BOOST_PIN)` helper configures both PA_DAC and overcurrent
   protection. Choose its power setting after measuring the link margin.
7. **Remove credentials from version control.** `src/config.h` is currently
   tracked. If that history has been shared, rotate the exposed keys and move
   credentials into an ignored local file or build secrets. This change does
   not modify existing credentials or Git history.

## Validation

Build: `pio run -e genericL051C`.

`test/reading_interval_checks.cpp` contains C++14 compile-time checks of the
actual command parser, including boundary values, binary zero bytes, malformed
commands and unchanged state on rejection. Compile it with any C++14 compiler:

```sh
arm-none-eabi-g++ -std=c++14 -Wall -Wextra -Werror -Iinclude -fsyntax-only test/reading_interval_checks.cpp
```

Before deploying, test on a device and gateway:

- Receive a 30-minute command in RX1 and in RX2; verify the next sleep duration.
- Include FOpts in a downlink; check that FPort and payload still decode correctly.
- Send invalid commands; verify the interval stays unchanged.
- Disconnect the temperature probe; check the `FFFF` uplink, sleep and downlink access.
- Reconnect the probe and verify recovery; power-cycle and verify the 10-minute default.
- Confirm join backoff during a gateway outage and measure sleep/RX/TX current.

Compilation and parser checks cannot establish radio timing or battery life.

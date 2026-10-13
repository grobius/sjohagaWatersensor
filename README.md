# sjohagaWatersensor

Battery-powered DS18B20 water temperature sensor using a MiniPill LoRa
(STM32L051C8T6 + RFM95W), EU868 and LoRaWAN OTAA.

## Build

Copy `src/config_example.h` to `src/config.h` for a new installation and enter the
OTAA credentials. `src/config.h` is ignored by Git, so credentials stay local;
preserve an existing configuration. Build with `pio run -e genericL051C`.

### Calibration library

PSLL includes its calibration wrapper through `lorawan.h`; the header lives at
`lib/PSLL/src/STM32LowPowerCal/STM32LowPowerCal.h`. The application uses that
public include instead of the former standalone `STM32LowPowerCal.h` path.
At startup, `LowPowerCal.calibrateRTC()` measures eight seconds of RTC progress
against the CPU's SysTick clock. The measured factor is used for receive-window
waits, join retry sleeps and the reading interval. The eight-second measurement
also leaves time to attach a programmer before low-power operation.
`lora.join()` preserves this factor: measuring join processing with `millis()`
alone does not measure RTC drift. An old `calTimeDivider` in `config.h` is unused.

RX1 and RX2 are scheduled from the RTC timestamp at the end of transmission,
with a 20 ms early-wake margin. RX2 includes time already spent receiving in RX1.
The former 80 ms lead could expire the approximately 66 ms SF7 receive window
before a downlink started. Verify reception on hardware, especially if changing
data rates or operating over a wide temperature range.

Radio logging is disabled by default in `lib/PSLL/src/LoRaWAN/Config.h`. If you
enable `DEBUG_LORA` or `DEBUG_LOW_LEVEL`, also enable `DEBUG_MAIN` in
`src/main.cpp` so that `Serial2` is defined and initialized.

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

### If no join requests appear

Check that the active `devEui`, `appEui` and `appKey` in `src/config.h` match the
device selected in TTN. This tracked file can change when pulling another branch.
Do not post the AppKey in logs or screenshots. A different DevEUI sends requests
for a different device, so inspect the gateway's live data as well.

Uncomment `DEBUG_MAIN` in `src/main.cpp`, rebuild and flash to see startup
diagnostics on PA2 (TX), PA3 (RX), at 9600 baud. The log shows the configured
DevEUI, radio initialization, each join attempt and its result. The AppKey is not
printed by these application diagnostics. Allow eight seconds after reset for
startup. After failed joins, retries back off from 15 seconds to one hour plus
jitter, so there can be a long quiet period.

EU868 join channel selection now chooses directly from channels 0–2. Previously,
repeatedly reseeding from a stable analog reading could keep selecting a channel
outside that range and block the first transmission. Nonce generation now wakes
the radio without reinitializing it, preserving the selected transmit power.
TX and RX interrupt waits have a 15-second software timeout; a TX timeout makes
the join attempt fail and return to the application's retry loop.

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

Use [tools/ttn-uplink-decoder.js](tools/ttn-uplink-decoder.js) as the JavaScript
uplink formatter in The Things Stack. It retains `vcc` in volts and `temperature`
in degrees Celsius, and maps `FFFF` to `temperature: null, sensor_ok: false`.
For example, `0C D1 FF FF` means 3.281 V and an unavailable temperature, not
605.35 °C. Check the DS18B20 connection on PA1, its supply and pull-up if this
persists. Sensor failure does not disable uplinks or downlink processing.

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
4. **Extend radio fault recovery.** TX and RX waits now have software timeouts,
   and failed join transmissions return to the retry loop. Automatic hardware
   reset/reinitialization after repeated radio failures remains a useful follow-up.
   A watchdog is useful only with a plan for feeding it during the
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
   The application now uses `setTxPower(2, PA_BOOST_PIN)` to configure PA_DAC
   and overcurrent protection as well as output power. Nonce generation preserves
   this setting. Adjust transmit power only after measuring the link margin.
7. **Rotate credentials exposed in Git history.** `src/config.h` is no longer
   tracked, but earlier commits still contain the AppKeys. If that history has
   been shared, rotate the exposed keys in the network console and reflash.

## Validation

Build: `pio run -e genericL051C`.

`test/reading_interval_checks.cpp` contains C++14 compile-time checks of the
actual command parser, including boundary values, binary zero bytes, malformed
commands and unchanged state on rejection. Compile it with any C++14 compiler:

```sh
arm-none-eabi-g++ -std=c++14 -Wall -Wextra -Werror -Iinclude -fsyntax-only test/reading_interval_checks.cpp
arm-none-eabi-g++ -std=c++14 -Wall -Wextra -Werror -Ilib/PSLL/src/STM32LowPowerCal -fsyntax-only test/sleep_timing_checks.cpp
```

The sleep timing checks cover fast/slow RTCs, RX1/RX2 deadlines, timestamp
rollover and avoiding an indefinite sleep when a deadline has already passed.

Before deploying, test on a device and gateway:

- Receive a 30-minute command in RX1 and in RX2; verify the next sleep duration.
- Include FOpts in a downlink; check that FPort and payload still decode correctly.
- Send invalid commands; verify the interval stays unchanged.
- Disconnect the temperature probe; check the `FFFF` uplink, sleep and downlink access.
- Reconnect the probe and verify recovery; power-cycle and verify the 10-minute default.
- Confirm join backoff during a gateway outage and measure sleep/RX/TX current.

Compilation and parser checks cannot establish radio timing or battery life.

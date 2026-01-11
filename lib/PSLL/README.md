# Plain and Simple LoRaWAN Library (in short PSLL)
Main goal of this library is to have simple code that is readable and adjustable by most arduino programmers. 
Furthermore to get a better understanding how LoRa works, so jou can fit it to you project needs.
And of course to reduce the power as much as possible when doing nothing.

## Developed in PlatformIO and with STM32L051 processor
Tested on my own board named MiniPill LoRa. Minimal components to get it working.

## Setup of this LoRaWAN library
main setup is:

```
+-------------------+
|   Application     |
+---------+---------+
          |
+---------+---------+   +---------------------------+
|    LoRaWan lib    +---+  STM32LowPowerCal (incl)  |
+---------+---------+   +---------+-----------------+
          |	                      |
+---------+---------+   +---------+----------+
|    LoRaMAC lib    |   | STM32LowPower (ext)| 
+---------+---------+   |   STM32RTC (ext)   |
          |             +--------------------+
+---------+---------+
|     RFM95 lib     |
+---------+---------+		  
          |
+---------+---------+
|   RFM95 hardware  |
+-------------------+
```
## Depending Libraries (install before use)
Extra libraries you should install/add before use

- https://github.com/stm32duino/STM32LowPower
- https://github.com/stm32duino/STM32RTC.git


## Main (new) features on version 2.1
- Randomizing with RSSI measurements from RFM chip to generate DevNonce.
- More robust and reliable Join procedure, no power down during Join procedure
- Calibrating internal clock during join procedure an using the 5 second delay of join packet as reference
- Updating with SPI.end() for low Power and SPI.begin() when waking up the RFM module.

## Tested/using conditions
Test conditions: 
MiniPill LoRa Board with STM32L051C8T6 controller with RFM95W module
TTN network (The Netherlands)
EU_868 band
PlatformIO Core 6.1.18·Home 3.4.4 on Visual Studio Code
STM32 library: v19.4.0

1. Join (default 5 seconds Receive Join1 delay)
2. Response to ADR Mac Command 0x06
3. Send uplink message
4. Unconfirmed downlink message (RX1 window)
5. Confirmed downlink message (RX1 window)
6. DEBUG and disable DEBUG
7. RX1 1 second and 5 seconds
8. NO_RX possibility for lowpower
9. 32 bit TX_Framecounter

## Features 
- Added DEBUG possibilities
- Added security/config file - do not upload this one to your git-repository!
- Added ADR return message for 0x06 Request
- Improved DevNonce calculation/randomization
- Mac command processing
- Confirmed downlink response
- Low Power mode, during Cycle (send and receive RX1 or RX2)
- No RX possibility for even more low power, use CLASS_A_NO_RX and disable ADR and periodic updates on LoRaWAN server side
- Clock timing correction
- Using RFM95 radio signal/noise RSSI measurement for randomization

## Updates in time
- @2020-xx-xx working on first version for STM32L051 processor
- @2022-xx-xx first version publicated
- @2022-12-11 added Wrapper for RTC callibration arround STM32LowPower
- @2025-09-17 updated version PSLL 1.3 
- @2025-10-02 updated version PSLL 1.4
- @2025-10-20 updated version PSLL 1.5
- @2025-10-23 updated version PSLL 2.0 - working version with RSSI randomization, time calibration with join timing
- @2025-10-27 updated version PSLL 2.1 - working version with latest platformIO ST STM32 framework

## Calibrated Time
Due to not using a crystal (less components), but the RC internal clock of the microcontroller, the timing is not correct. 
However as from version 1.4 I use the LoRaWAN timing at join to get the correct timing by using the join period/RX_1 window to calculate 
the factor between millis and the real know RX_1 window time.

## Scope
This is a basic LoRaWAN Library for Arduino based code and devices, tested on ARM based STM32L051C8T6 device
Primary build for Experimental en Educational purposes
Using the SX1272, SX1276 transceivers and compatible modules (such as some HopeRF RFM9x modules).
library supports LoRaWAN Class Aand Class C implementations operating in EU-868, AU-915, US-915 and AS-923 bands, although not all classes and bands are tested. 
The LoRaWAN 1.0.3 Documentation and Regional Parameters Documentation is used as basis for this library

## Now working with version 19.4.0 problem found!
The issue was that the SPI was ended in low power mode and not started afterwards in the library from version > 15.6.0. 

This application worked before the fix with PlatformIO/ST STM32 framework version 15.6.0, however not with higer versions and the latest 19.4.0 (okt 2025)
The problem is that after join, the first packet is send. After that the RFM tries to receive one packet. However, the DIO1 pin does not get high.
This is measured with a logic analyser.
The SPI commands send to the RFM module is the same as normal situation. (setting all DEBUG on, also low level where you can see the SPI commands)


## Scope of this library
This is a basic library with NOT all functionality in as decribed in the LoRaWAN 1.0.3 documentation.
However the aim was to create a library that is readable, debugable and very basic.
The problem I encountered with the LMIC library is that it uses a RTOS like design. For most beginning Arduino programmers the code
is not readable and debugable.
The aim of this code is to provide a simple setup and hence easy extenable and improvable.

## On the shoulders of other developers
This code and drivers is based on work from:
- Gerben den Hartog [ideetron] (most of the low level stuff)
- Andri Rahmadhani (lorawan-arduino-rfm Wrapper for Gerben's code)
- Ivan Moreno [Beelan] (examples and test)
- Eduardo Contreras [Beelan] (examples and test)

## History
This repository is all based on the library originally created by Ideetron B.V. This library is slightly
modified and this [repo](https://git.antares.id/lorawan-loraid/arduino-loraid), with the principal purpose to have an LoRaWAN MAC layer for arduino with an easy API. The basic of het BLLIC library is the [repo](https://github.com/BeelanMX/Beelan-LoRaWAN) with its examples and documentation.



## Information from original Beelan README.md (modified):

### What certainly works on Beelan version
 - Sending packets uplink, taking into account duty cycling.
 - Custom frequencies and datarate settings.
 - Receiving downlink packets in the RX1 window (EU-868, AS-923,US-915,AU-915).
 - Over-the-air activation (OTAA / joining) (EU-868, AS-923).
 - Class C operation.
 - Receiving downlink packets in the RX1 window (US-915).
 - Receiving downlink packets in the RX2 window.
 - Over-the-air activation (OTAA / joining) (US-915). 

### Configuration
A number of features can be configured or disabled by editing the `Config.h` file in the library folder. 
At the very least, you should set the right type of board in Config.h, most other values should be fine at their defaults. When using the US_915 you need to select which sub-band you will use, by default it is sub-band 6.

### Supported hardware
This library is intended to be used with plain LoRa transceivers, connecting to them using SPI. In particular, the SX1272 and SX1276 families are supported (which should include SX1273, SX1277, SX1278 and SX1279 which only differ in the available frequencies, bandwidths and spreading factors). It has been tested with both SX1272 and SX1276
chips, using the Semtech SX1272 evaluation board and the HopeRF RFM92 and RFM95 boards (which supposedly contain an SX1272 and SX1276 chip respectively).

This library has been tested using:

- Arduino Uno
- WeMos D1 R2 (ESP8266 family board)
- ESP32
- Electronic Cats CatWAN USB-Stick
- Electronic Cats Bast-WAN (Based on SAMR34)
- NINA B302 is NRF52840 core
- STMDuino

### Pin mapping
As described above, most connections can use arbitrary I/O pins on the
Arduino side. To tell the arduino LoRaWAN library about these, a pin mapping struct is used in the sketch file.

For example, this could look like this:
```
	sRFM_pins RFM_pins = {
	  	.CS = SS,
	  	.RST = RFM_RST,
	  	.DIO0 = RFM_DIO0,
	  	.DIO1 = RFM_DIO1,
	  	.DIO2 = RFM_DIO2,
	  	.DIO5 = RFM_DIO5
  	}; 
```

### API
See [API.md](docs/API.md) in the docs directory

### Tests
See [Test Folder](test/README.md) 

### License
Most source files in this repository are made available under the MIT License. The examples which use a more liberal
license. Some of the AES code is available under the LGPL. Refer to each individual source file for more details.


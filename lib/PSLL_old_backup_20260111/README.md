# Plain and Simple LoRaWAN Library (in short PSLL)
Main goal of this library is to have simple code that is readable and adjustable by most arduino programmers. 
Furthermore to get a better understanding how LoRa works, so jou can fit it to you project needs.
And of course to reduce the power as much as possible when doing nothing.

## Scope/Features
This is a basic LoRaWAN Library for Arduino based code and devices, tested on ARM based STM32L051C8T6 device
Primary build for Experimental en Educational purposes
Using the SX1272, SX1276 transceivers and compatible modules (such as some HopeRF RFM9x modules).
library supports LoRaWAN Class A and Class C implementations operating in EU-868, AU-915, US-915 and AS-923 bands, although not all classes and bands are tested.
The LoRaWAN 1.0.3 Documentation and Regional Parameters Documentation is used as basis for this library

## Aim of this library
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

## Tested
Test conditions: 
MiniPill LoRa (STM32L051C8T6) controller with RFM95W module
TTN network (The Netherlands)
EU_868 band
PlatformIO Core 6.1.4 - Home 3.4.3 on Visual Studio Code

1. Join (response within 2 join message) (default 5 seconds Receive Join1 delay)
2. Response to ADR Mac Command 0x06
3. Send uplink message
4. Unconfirmed downlink message (RX1 window)
5. Confirmed downlink message (RX1 window)
6. DEBUG and disable DEBUG
7. RX1 1 second and 5 seconds

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


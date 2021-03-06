AirWatch LoRa
=============

This project aims to build a cheap affordable air quality monitoring device. 
2 hardware version are provided, one for powering via 5v usb cable, the other for a solar powered version. 

Version 1 
--------------------

It uses the Adafruit Feather M0 RFM9X dev board. (ATSAMD21G18 ARM Cortex M0 processor and SX127x LoRa® based module)

It uses the following sensors:


BME280 - Temperature, humidity and altitude (pressure). Humidty effects the particle matter count of the PMS5003, hence why included.

PMS 5003 - Particle Matter. PM1.0, PM2.5 and PM10.0 concentration in both standard & enviromental units

MICS-6814 - C0, N02 and NH3 (Carbon Monoxide, Nitrogen Dioxide and Ammonia)


Particular attention is getting this device to work well in sleep mode, and last for as long as possible on batteries/solar. 
However, bear in mind the particle matter sensor (PMS5003) consists of a small fan, that must be run to suck in the air. This must be run for about 30 seconds, before taking a reading. 

The PMS5003 does have an enable pin (SET) which can be pulled low to turn the sensor (and fan) off. If we do not use this, apart fom using a lot of power, the PMS5003 will only last for 8000 hours of operation. So it is very important that our Feather M0, is turning the PMS5003 on and off. 


To do: Yet to focus on sleep mode for bme280, the lora radio chip itself, the mics-6814 ..

The MICS-6814 has a warm up period of 20 mins before readings are accurate. 

To do: Yet to focus on sleep mode for bme280, the lora radio chip itself, the mics-6814 ..



Non Solar Powered (power via mains)
-------------------------------------

![alt text](https://github.com/rorygleeson/AirWatch/blob/master/Devices/LoRa/LoRa-NonSolar.jpg)



 Solar Powered
-------------------------------------


![alt text](https://github.com/rorygleeson/AirWatch/blob/master/Devices/LoRa/LoRa-Solar.jpg)




AirWatch LoRa Hardware Bill of Materials
----------------------------------------

Adafruit Feather M0 RFMx ~ price: $34 USD


BME280P ~ average price: $2 USD


PMS 5003 ~ average price: $13 USD


Mics-6814 Chip ~ price: $23 USD

 

5V DC 1A POWER ADAPTER WITH MICRO USB PLUG ~ price:  $5 USD


Enclosure price: ~ $3 USD




1 X 3.3K Resistor
Connector wires
PCB solder board

Approx Total price Non Solar ~  $80  USD



Extra components required for solar:

Pololu 5V Step-Up Voltage Regulator  ~ price $4 USD 


Solar Lipo Charger (3.7V) average price $5 USD 


2 Solar Panel (9v 220mA) average price $7 USD 


Approx Total price Solar ~  $105  USD


Libraries:
==================


Adafruit BME280 Library
https://github.com/adafruit/Adafruit_BME280_Library

Adafruit Sleepy dog
https://github.com/adafruit/Adafruit_SleepyDog

MCCI LoRaWAN LMIC Library
https://github.com/mcci-catena/arduino-lmic


Adafruit unified sensor
https://github.com/adafruit/Adafruit_Sensor


12C-Senor-lib
https://github.com/orgua/iLib
































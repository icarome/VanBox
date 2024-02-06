# VanBox

## Description

ESP32 based device made to bridge the comunication between PSA cars with VAN bus to Android Head Units

This project can read from the VAN Bus using a TJA1050 transciever, decode the car data and send it over serial for an
Android Head Unit simulating a CanBox.

## Instalation

This project can be cloned and installed into the ESP32 using the platformIO

## Hardware

This project uses:
- ESP32 board
- TJA1050 transciever
- RF433 Transmitter Module

The RX pin of the transciever is connected to pin 4 of the Dev Board
The TX pin of the module is not used in this project

The CAN High pin of the transciever should be connected to pin 4 of the LCD display of the car
The CAN Low pin of the transciever should be connected to pin 17 of the LCD display of the car

Pin 17 of the ESP32 Dev Board should be connected to the TX pin of Android Head Unit


## Liability

This software and hardware was tested on a Peugeot 207. Should work on any PSA car with VAN bus.

I cannot take any responsibility if something goes wrong if you build it and install it in your car. So use it at your own risk.

### Based On

This work was built on top of existing projects such as:
- https://github.com/icarome/VwRaiseCanbox
- https://github.com/morcibacsi/esp32_rmt_van_rx
- https://github.com/0xCAFEDECAF/VanBus


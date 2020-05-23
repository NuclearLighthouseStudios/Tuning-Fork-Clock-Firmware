# Tuning Fork Clock Firmware

This repository contains the firmware for the Tuning Fork Clock.

The Makefile can build a .hex file and also flash it to the chip using avrdude using `make flash`.

Running `make fuses` will set all the correct fuse bits on the controller using avrdude.
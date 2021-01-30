# Tuning Fork Clock Firmware

This repository contains the firmware for the Tuning Fork Clock.

The Makefile can build a .hex file and also flash it to the chip using avrdude using `make flash`.

Running `make fuses` will set all the correct fuse bits on the controller using avrdude.

## Serial Protocol

The firmware exposes a serial interface at 9600 baud which allows you to remotely configure the clock.

### Available Commands

All commands consist of a single upper case letter specifying an option directly followed by a value or a question mark and terminated with a newline.  
Passing a value will set the option to that value, passing a question mark will return the current value of that option.

Note that some options are read-only or write-only.

Each command is followed by a line beginning with an equals sign followed either by the letters `OK` or `ERR`.  
The error response is followed by an error code.

#### `E`

Controls echoing of characters. This value is write only.

* `E0\n` - Disable echoing
* `E1\n` - Enable echoing


#### `A`

Controls automatic time sending. This value is write only.

When enabled the current time will be sent via serial port each second.

* `A0\n` - Disable automatic time sending
* `A1\n` - Enable automatic time sending


#### `C`

Controls the charset. This value is write only.

* `C0\n` - Use PAWScript
* `C1\n` - Use normal boring numbers


#### `T`

Controls the current time.

When read it returns the current time with second accuracy, when written it only accepts hours and minutes.

* `T?\n` - Returns the current time in HH:MM:SS format
* `T12:34\n` - Set the time to 12:34


#### `P`

Controls the pulses per minute setting.

* `P?\n` - Returns the current pulses per minute setting
* `P26400\n` - Set the time to 26400


#### `M`

Retrieves the mode of the clock. This value is read-only.

* `M?\n` - Returns the current mode the clock is in

Possible Modes:

* `D` - Time display mode
* `H` - Hour setting mode
* `M` - Minute setting mode
* `T` - Pulses per minute tuning mode
* `E` - Error mode due to loss of tuning fork signal or other error


#### `R`

Controls the "run" state of the clock. It is not possible to stop the clock with this command.

* `R?\n` - Returns 0 or 1 depending on if the clock is current running or not
* `P1\n` - Sets the clock running, clearing any errors or time-set modes


### Error Codes

* `:UC` - Unknown command
* `:INV` - Invalid command parameter
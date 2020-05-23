HEX = clock.hex
BINARY = clock.elf
OBJECTS = clock.o

CC = avr-gcc
LD = avr-gcc
OC = avr-objcopy

CFLAGS = -g -flto -Os -Wall -Wextra -Werror -mmcu=attiny4313
LDFLAGS = -g -flto -mmcu=attiny4313
OCFLAGS = -j .text -j .data -O ihex

all: $(HEX)

$(HEX) : $(BINARY)
	$(OC) $(OCFLAGS) $^ $@

$(BINARY) : $(OBJECTS)
	$(LD) $(LDFLAGS) $^ -o $@

%.o: %.c
	$(CC) $(CFLAGS) -c $<


clock.o: clock.c chars_human.h


.PHONY: flash fuses clean
flash: $(HEX)
	avrdude -c stk500 -p t4313 -P /dev/ttyACM0 -U flash:w:$<

fuses:
	avrdude -c stk500 -p t4313 -P /dev/ttyACM0 -U lfuse:w:0x64:m -U hfuse:w:0x99:m

clean:
	rm -f $(HEX) $(BINARY) $(OBJECTS)
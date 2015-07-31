MCU = atmega328p
F_CPU = 16000000L

PROG = arduino
PORT = /dev/ttyAMA0
UP_BAUD = 115200

CC = avr-gcc
OBJCOPY = avr-objcopy

CFLAGS = -mmcu=$(MCU) -DF_CPU=$(F_CPU) -Os -Wall -Wstrict-prototypes -std=gnu99 -funsigned-char -funsigned-bitfields -fpack-struct -fshort-enums
LDFLAGS = -Wl,--gc-sections

all: main.hex

%.elf: main.o serial.o
	$(CC) $(CFLAGS) $(LDFLAGS) -o $@ $^

clean:
	rm -rf *.o *.elf *.lst *.map *.sym *.lss *.eep *.srec *.bin *.hex *.tmp.sh

%.hex: %.elf
	$(OBJCOPY) -j .text -j .data -O ihex $< $@
	
upload: all
	avrdude -c $(PROG) -p $(MCU) -P $(PORT) -b $(UP_BAUD) -Uflash:w:main.hex:i

fuse:
	avrdude -c $(PROG) -p $(MCU) -P $(PORT) -b $(UP_BAUD) -U lfuse:w:0xCE:m -U hfuse:w:0xD9:m -U efuse:w:0x07:m -U lock:w:0x3F:m

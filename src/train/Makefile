MCU = attiny84
F_CPU = 8000000L

PROG = arduino
PORT = com5
UP_BAUD = 19200

CC = avr-gcc
OBJCOPY = avr-objcopy

CFLAGS = -mmcu=$(MCU) -DF_CPU=$(F_CPU) -Os -Wall -Wstrict-prototypes -std=gnu99 -funsigned-char -funsigned-bitfields -fpack-struct -fshort-enums
LDFLAGS = -Wl,--gc-sections

all: main.hex

%.elf: main.o
	$(CC) $(CFLAGS) $(LDFLAGS) -o $@ $<

clean:
	rm -rf *.o *.elf *.lst *.map *.sym *.lss *.eep *.srec *.bin *.hex *.tmp.sh

%.hex: %.elf
	$(OBJCOPY) -j .text -j .data -j .version --set-section-flags .version=alloc,load -O ihex $< $@
	
upload: all
	avrdude -c $(PROG) -p $(MCU) -P $(PORT) -b $(UP_BAUD) -Uflash:w:main.hex:i

fuse:
	avrdude -c $(PROG) -p $(MCU) -P $(PORT) -b $(UP_BAUD) -U lfuse:w:0xE2:m -U hfuse:w:0xDF:m -U efuse:w:0xFF:m -U lock:w:0xFF:m

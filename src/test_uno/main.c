#include <avr/io.h>
#include <avr/interrupt.h>
#include <util/delay.h>
#include "../twpc_def.h"

/*

Model railway train code
created by L Szabi 2015

Code for trains
- TWPC receive only
- Onewire client
- LED
- Motor

*/

#define TWPC_UID 21

#define LED_P 5
#define LED_PORT PORTB
#define LED_DDR DDRB

#define TWPC_P 0
#define TWPC_PORT PORTB
#define TWPC_PIN PINB
#define TWPC_DDR DDRB

#define ONEWIRE_P 6
#define ONEWIRE_PORT PORTD
#define ONEWIRE_PIN PIND
#define ONEWIRE_DDR DDRD

// LED driver

void led_on(void) {
	LED_PORT |= _BV(LED_P);
}

void led_off(void) {
	LED_PORT &= ~_BV(LED_P);
}

void led_init(void) {
	LED_DDR |= _BV(LED_P);
	led_off();
}

void led_blink(void) {
	led_on();
	_delay_ms(50);
	led_off();
}

// Communication

static volatile int twpc_even = 1;

static volatile int twpc_bit = 0;

static volatile int onewire_bit = 0;
static volatile int onewire_even = 0;

volatile twpc_packet_t com_data;
volatile int com_received = 0;

// inverted logic
static int twpc_read(void) {
	TWPC_DDR &= ~_BV(TWPC_P);
	return !( TWPC_PIN & _BV(TWPC_P) ) ? 1 : 0;
}

static int onewire_read(void) {
	ONEWIRE_DDR &= ~_BV(ONEWIRE_P);
	ONEWIRE_PORT &= ~_BV(ONEWIRE_P);
	return ( ONEWIRE_PIN & _BV(ONEWIRE_P) ? 1 : 0 );
}

static void onewire_line_off(void) {
	ONEWIRE_DDR |= _BV(ONEWIRE_P);
	ONEWIRE_PORT &= ~_BV(ONEWIRE_P);
}

static void onewire_line_on(void) {
	ONEWIRE_DDR |= _BV(ONEWIRE_P);
	ONEWIRE_PORT |= _BV(ONEWIRE_P);
}

void com_init(void) {
	OCR1A = 30; // 0.5ms
	TCCR1A = 0;
	TCCR1B = _BV(WGM12) | _BV(CS12); // F_CPU/256, CTC mode
	TIMSK1 = _BV(OCIE1A);
}

ISR(TIMER1_COMPA_vect) {
	// code for onewire
	if ( onewire_even ) {
		if ( onewire_bit == 0 ) {
			if ( onewire_read() ) {
				onewire_bit = 1;
			}
		} else if ( onewire_bit == 1 ) {
			onewire_line_on();
			onewire_bit = 2;
		} else if ( onewire_bit > 1 && onewire_bit < 10 ) {
			if ( TWPC_UID & _BV(onewire_bit - 2) ) {
				onewire_line_on();
			} else {
				onewire_line_off();
			}
			onewire_bit++;
		} else if ( onewire_bit == 10 ) {
			onewire_line_on(); // error checking
			onewire_bit++;
		} else {
			onewire_read();
			onewire_bit = 0;
		}
	} else {
		if ( onewire_bit == 0 && onewire_read() ) {
			onewire_bit = 1;
			onewire_even = 1; // sync
		}
	}
	// code for twpc
	if ( twpc_even ) {
		// code for receiving
		if ( twpc_bit == 0 ) { // first start bit
			if ( twpc_read() ) {
				twpc_bit = 1;
			}
		} else if ( twpc_bit == 1 ) { // second start bit
			if ( !twpc_read() ) {
				twpc_bit = 2;
				com_data.data_raw = 0;
			} else {
				twpc_bit = 0; // failed
			}
		} else if ( twpc_bit >= 2 && twpc_bit < 2 + TWPC_DATA_BITS ) { // read data
			com_data.data_raw |= twpc_read() << ( twpc_bit - 2 );
			twpc_bit++;
		} else {
			twpc_bit = 0;
			com_received = 1;
		}
	} else {
		// code for receiving ( synchronize )
		if ( twpc_bit == 1 && !twpc_read() ) {
			twpc_bit = 2;
			com_data.data_raw = 0;
			twpc_even = 1; // turn parity
		}
	}
	twpc_even ^= 1;
	onewire_even ^= 1;
}

// Main

int main(void) {
	cli();
	led_init();
	com_init();
	sei();
	while ( 1 ) {
		if ( com_received ) {
			if ( com_data.uid == TWPC_UID || com_data.uid == 255 ) {
				if ( com_data.cmd == TWPC_CMD_LIGHT_ON ) {
					led_on();
				} else if ( com_data.cmd == TWPC_CMD_LIGHT_OFF ) {
					led_off();
				}
			}
			com_received = 0;
		}
	}
	return 1;
}
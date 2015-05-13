#include <avr/io.h>
#include <avr/interrupt.h>
#include <avr/pgmspace.h>
#include <util/delay.h>

/*

New Model Railway Master code
created by L Szabi 2015

*/

#define LED_P 7
#define LED_PORT PORTB
#define LED_DDR DDRB

#define ONEWIRE_DDR DDRC
#define ONEWIRE_PORT PORTC
#define ONEWIRE_PIN PINC

#define TWPC_POWER_P 6
#define TWPC_POWER_DDR DDRB
#define TWPC_POWER_PORT PORTB

#define TWPC_DATA_P 4
#define TWPC_DATA_DDR DDRE
#define TWPC_DATA_PIN PINE

#define TWPC_DATA_BITS 8

static const int onewire_pins[] = { 0, 1 }; // multiple places
static const int onewire_pins_n = 2;

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

#define TWPC_STATE_IDLE 0
#define TWPC_STATE_SEND 1
#define TWPC_STATE_RECV 2

static volatile int twpc_even = 1;

static volatile int twpc_bit = 0;
static volatile int twpc_data = 0;
static volatile int twpc_received = 0;

static volatile int twpc_state = 0;

static volatile int onewire_even = 0;
static volatile int onewire_idx = 0;
static volatile int onewire_bit = 0;

static volatile int onewire_pin = 0;
static volatile int onewire_dev = 0;
static volatile int onewire_got = 0;

// using inverted logic: '0' = +Vcc, '1' = 0V
static void twpc_line_on(void) {
	TWPC_POWER_PORT |= _BV(TWPC_POWER_P);
}

static void twpc_line_off(void) {
	TWPC_POWER_PORT &= ~_BV(TWPC_POWER_P);
}

static int twpc_read(void) {
	return !( TWPC_DATA_PIN & _BV(TWPC_DATA_P) ) ? 1 : 0;
}

static int onewire_read(void) {
	ONEWIRE_DDR &= ~_BV(onewire_pins[onewire_idx]);
	ONEWIRE_PORT &= ~_BV(onewire_pins[onewire_idx]);
	return ( ONEWIRE_PIN & _BV(onewire_pins[onewire_idx]) ? 1 : 0 );
}

static void onewire_line_off(void) {
	ONEWIRE_DDR |= _BV(onewire_pins[onewire_idx]);
	ONEWIRE_PORT &= ~_BV(onewire_pins[onewire_idx]);
}

static void onewire_line_on(void) {
	ONEWIRE_DDR |= _BV(onewire_pins[onewire_idx]);
	ONEWIRE_PORT |= _BV(onewire_pins[onewire_idx]);
}

static void onewire_next(void) {
	onewire_idx = ( onewire_idx + 1 ) % onewire_pins_n;
}

void com_init(void) {
	OCR1A = 30; // 0.5ms
	TCCR1A = 0;
	TCCR1B = _BV(WGM12) | _BV(CS12); // F_CPU/256, CTC mode
	TIMSK1 = _BV(OCIE1A);
	TWPC_POWER_DDR |= _BV(TWPC_POWER_P);
	TWPC_DATA_DDR &= ~_BV(TWPC_DATA_P);
}

void com_send(int data) {
	twpc_data = data;
	twpc_bit = 0;
	twpc_state = TWPC_STATE_SEND;
}

int com_recv(void) {
	twpc_bit = 0;
	twpc_received = 0;
	twpc_state = TWPC_STATE_RECV;
	while ( twpc_received == 0 );
	return twpc_data;
}

ISR(TIMER1_COMPA_vect) {
	// code for onewire
	if ( onewire_even ) {
		if ( onewire_bit == 0 ) { // send first start bit
			onewire_line_on();
			onewire_bit = 1;
		} else if ( onewire_bit == 1 ) { // float line
			onewire_read();
			onewire_bit = 2;
		} else if ( onewire_bit == 2 ) { // wait for second start bit
			if ( onewire_read() ) { // accept data
				onewire_bit = 3;
				onewire_pin = onewire_idx;
				onewire_dev = 0;
			} else {
				onewire_bit = 0;
				onewire_next();
			}
		} else if ( onewire_bit > 2 && onewire_bit < 11 ) {
			onewire_dev |= onewire_read() << ( onewire_bit - 3 );
			onewire_bit++;
		} else {
			if ( onewire_read() ) { // error checking
				onewire_got = 1;
			}
			onewire_bit = 0;
			onewire_next();
		}
	}
	// code for twpc
	if ( twpc_even ) {
		// code for sending
		if ( twpc_state == TWPC_STATE_SEND ) { // is there data to send?
			if ( twpc_bit == 0 ) {
				twpc_line_on(); // first start bit '1'
			} else if ( twpc_bit == 1 ) {
				twpc_line_off(); // second start bit '0'	
			} else if ( twpc_bit >= 2 && twpc_bit < 2 + TWPC_DATA_BITS ) { // send data
				if ( twpc_data & _BV(twpc_bit - 2) ) {
					twpc_line_on();
				} else {
					twpc_line_off();
				}
			} else {
				twpc_line_off();
				twpc_state = TWPC_STATE_IDLE; // data sent
			}
			twpc_bit++;
		}
	} else {
		// code for receiving
		if ( twpc_state == TWPC_STATE_RECV ) {
			if ( twpc_bit == 0 ) { // send first start bit
				twpc_line_on(); // '1' -> no voltage on line
				twpc_bit = 1;
			} else if ( twpc_bit == 1 ) { // waiting for second start bit
				if ( !twpc_read() ) {
					twpc_bit = 2;
					twpc_data = 0;
				}
			} else if ( twpc_bit >= 2 && twpc_bit < 2 + TWPC_DATA_BITS ) { // read data
				twpc_data |= twpc_read() << ( twpc_bit - 2 );
				twpc_bit++;
			} else {
				twpc_line_off();
				twpc_received = 1;
				twpc_state = TWPC_STATE_IDLE;
			}
		}
		
	}
	twpc_even ^= 1;
	onewire_even ^= 1;
}

// Main

void dbg(int t) {
	PORTB |= _BV(4);
	_delay_ms(t);
	PORTB &= ~_BV(4);
}

int main(void) {
	cli();
	led_init();
	com_init();
	DDRB |= _BV(4) | _BV(5); // debug
	sei();
	while ( 1 ) {
		if ( onewire_got ) {
			onewire_got = 0;
			if ( onewire_dev != 21 ) {
				led_blink();
			}
			com_send(onewire_dev);
		}
		/*
		dbg(2);
		com_send(199);
		_delay_ms(30);
		int d = com_recv();
		dbg(1);
		if ( d == 85 ) {
			_delay_ms(2);
			dbg(1);
		}
		_delay_ms(30);
		*/
	}
	return 1;
}

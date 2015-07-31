#include <avr/io.h>
#include <avr/interrupt.h>
#include <avr/pgmspace.h>
#include <util/delay.h>
#include "serial.h"
#include "../twpc_def.h"

/*

Model Railway Master code
created by L Szabi 2015

Code for master station
- TWPC master
- Onewire server
- LED
- UART

*/

#define LED_P 5
#define LED_PORT PORTB
#define LED_DDR DDRB

#define ONEWIRE_DDR DDRC
#define ONEWIRE_PORT PORTC
#define ONEWIRE_PIN PINC

#define TWPC_POWER_P 1
#define TWPC_POWER_DDR DDRB
#define TWPC_POWER_PORT PORTB

#define TWPC_DATA_P 2
#define TWPC_DATA_DDR DDRB
#define TWPC_DATA_PIN PINB

#define TWPC_FAULT_THRESHOLD 10

#define ONEWIRE_PINS_N 1

#if ONEWIRE_PINS_N > 0
static const int onewire_pins[] = { 0 }; // multiple places
#endif

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
static volatile int twpc_state = 0;
static volatile int twpc_fault = 0;

#if ONEWIRE_PINS_N > 0
static volatile int onewire_even = 0;
static volatile int onewire_idx = 0;
static volatile int onewire_bit = 0;
static volatile int onewire_data = 0;

static volatile int onewire_pin = 0;
static volatile int onewire_dev = 0;
static volatile int onewire_got = 0;

static volatile int onewire_last[ONEWIRE_PINS_N];
#endif

static volatile twpc_packet_t twpc_data_send;
static volatile int twpc_sent = 1;

volatile twpc_packet_t twpc_data_recv;
volatile int twpc_received = 0;

// using inverted logic: '0' = +Vcc, '1' = 0V
static void twpc_line_off(void) {
	TWPC_POWER_PORT |= _BV(TWPC_POWER_P);
}

static void twpc_line_on(void) {
	TWPC_POWER_PORT &= ~_BV(TWPC_POWER_P);
}

static uint32_t twpc_read(void) {
	return !( TWPC_DATA_PIN & _BV(TWPC_DATA_P) ) ? 1UL : 0UL;
}

#if ONEWIRE_PINS_N > 0
static int onewire_read(void) {
	ONEWIRE_DDR &= ~_BV(onewire_pins[onewire_idx]);
	ONEWIRE_PORT &= ~_BV(onewire_pins[onewire_idx]);
	return ( ONEWIRE_PIN & _BV(onewire_pins[onewire_idx]) ? 1 : 0 );
}

/* not used
static void onewire_line_off(void) {
	ONEWIRE_DDR |= _BV(onewire_pins[onewire_idx]);
	ONEWIRE_PORT &= ~_BV(onewire_pins[onewire_idx]);
} */

static void onewire_line_on(void) {
	ONEWIRE_DDR |= _BV(onewire_pins[onewire_idx]);
	ONEWIRE_PORT |= _BV(onewire_pins[onewire_idx]);
}

static void onewire_next(void) {
	onewire_idx = ( onewire_idx + 1 ) % ONEWIRE_PINS_N;
}
#endif

void com_init(void) {
	OCR1A = 30; // 0.5ms
	TCCR1A = 0;
	TCCR1B = _BV(WGM12) | _BV(CS12); // F_CPU/256, CTC mode
	TIMSK1 = _BV(OCIE1A);
	TWPC_POWER_DDR |= _BV(TWPC_POWER_P);
	TWPC_DATA_DDR &= ~_BV(TWPC_DATA_P);
	twpc_line_off();
#if ONEWIRE_PINS_N > 0
	for ( int i = 0; i < ONEWIRE_PINS_N; i++ ) {
		onewire_last[i] = 0;
	}
#endif
}

void com_send(twpc_packet_t *packet) {
	while ( !twpc_sent );
	twpc_sent = 0;
	twpc_data_send.data_raw = packet->data_raw;
	twpc_bit = 0;
	twpc_state = TWPC_STATE_SEND;
}

void com_recv(void) {
	while ( !twpc_sent );
	twpc_bit = 0;
	twpc_received = 0;
	twpc_state = TWPC_STATE_RECV;
	while ( twpc_received == 0 );
}

ISR(TIMER1_COMPA_vect) {
#if ONEWIRE_PINS_N > 0
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
				onewire_data = 0;
			} else {
				onewire_bit = 0;
				onewire_next();
			}
		} else if ( onewire_bit > 2 && onewire_bit < 11 ) {
			onewire_data |= onewire_read() << ( onewire_bit - 3 );
			onewire_bit++;
		} else {
			if ( onewire_read() && onewire_last[onewire_idx] != onewire_data ) { // error checking
				onewire_pin = onewire_idx;
				onewire_dev = onewire_data;
				onewire_last[onewire_idx] = onewire_data;
				onewire_got = 1;
			}
			onewire_bit = 0;
			onewire_next();
		}
	}
	onewire_even ^= 1;
#endif
	// code for twpc
	if ( twpc_even ) {
		// code for sending
		if ( twpc_state == TWPC_STATE_SEND ) { // is there data to send?
			if ( twpc_bit == 0 ) {
				twpc_line_on(); // first start bit '1'
			} else if ( twpc_bit == 1 ) {
				twpc_line_off(); // second start bit '0'
			} else if ( twpc_bit >= 2 && twpc_bit < 2 + TWPC_DATA_BITS ) { // send data
				if ( twpc_data_send.data_raw & ( 1UL << (twpc_bit - 2) ) ) {
					twpc_line_on();
				} else {
					twpc_line_off();
				}
			} else if ( twpc_bit == 2 + TWPC_DATA_BITS ) {
				twpc_line_off();
				twpc_bit++;
			} else {
				twpc_sent = 1;
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
				twpc_even ^= 1;
				twpc_fault = 0;
			} else if ( twpc_bit == 1 ) { // waiting for second start bit
				if ( !twpc_read() ) {
					twpc_data_recv.data_raw = 0;
					twpc_bit = 2;
				} else {
					twpc_even ^= 1; // loop back
#if TWPC_FAULT_THRESHOLD > 0
					twpc_fault++;
					if ( twpc_fault > TWPC_FAULT_THRESHOLD ) {
						twpc_line_off();
						twpc_received = 1;
						twpc_state = TWPC_STATE_IDLE;
					}
#endif
				}
			} else if ( twpc_bit >= 2 && twpc_bit < 2 + TWPC_DATA_BITS ) { // read data
				twpc_data_recv.data_raw |= twpc_read() << ( twpc_bit - 2 );
				twpc_bit++;
			} else if ( twpc_bit == 2 + TWPC_DATA_BITS ) {
				twpc_line_off();
				twpc_bit++;
			} else {
				twpc_received = 1;
				twpc_state = TWPC_STATE_IDLE;
			}
		}
	}
	twpc_even ^= 1;
}

// Main

static int read_nibble(char c) {
	if ( c >= '0' && c <= '9' ) {
		return c - '0';
	} else if ( c >= 'a' && c <= 'f' ) {
		return c - 'a' + 0x0a;
	} else if ( c >= 'A' && c <= 'F' ) {
		return c - 'A' + 0x0a;
	}
	return 0;
}

static char write_nibble(int x) {
	if ( x >= 0 && x < 10 ) {
		return '0' + x;
	} else if ( x >= 0x0a && x <= 0x0f ) {
		return 'a' + x - 0x0a;
	}
	return '0';
}

int read_byte(char first, char second) {
	return read_nibble(first) << 4 | read_nibble(second);
}

void write_byte(char *s, int x) {
	s[0] = write_nibble((x & 0xF0) >> 4);
	s[1] = write_nibble(x & 0x0F);
}

void write_int(char *s, uint32_t x) {
	write_byte(s, x >> 24);
	write_byte(&s[2], x >> 16);
	write_byte(&s[4], x >> 8);
	write_byte(&s[6], x);
	s[8] = '\0';
}

int main(void) {
	cli();
	led_init();
	com_init();
	serial_init(57600);
	led_blink();
	twpc_packet_t last_packet;
	sei();
	while ( 1 ) {
		// Receive data from uart
		if ( serial_available() >= sizeof(twpc_packet_t) ) {
			serial_gets((char *)&last_packet, sizeof(twpc_packet_t));
			if ( last_packet.checksum == TWPC_CHECKSUM(last_packet) ) {
				int ok = 0;
				//for ( int i = 0; i < 5; i++ ) {
					com_send(&last_packet);
					com_recv();
					/*if ( twpc_data_recv.checksum == TWPC_CHECKSUM(twpc_data_recv) && twpc_data_recv.cmd == TWPC_CMD_ACK ) {
						serial_puts("ok");
						serial_put(write_nibble(i));
						ok = 1;
						break;
					}*/
					char msg[16];
					write_int(msg, twpc_data_recv.data_raw);
					serial_puts(msg);
					twpc_data_recv.data_raw = 0;
				/*}
				if ( !ok ) {
					serial_puts("re");
				}*/
			} else {
				serial_puts("re");
				serial_flush_rx();
			}
		}
		if ( onewire_got ) {
			onewire_got = 0;
			char buf[10];
			write_byte(buf, onewire_dev);
			buf[2] = '\0';
			serial_put('w');
			serial_puts(buf);
		}
	}
	return 1;
}

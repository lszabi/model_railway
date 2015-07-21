#include <avr/io.h>
#include <avr/interrupt.h>
#include <util/delay.h>
#include <string.h>

/*

Model railway slave code
created by L Szabi 2015

(sample code for slave device)

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

// Main

#define TWPC_STATE_RECV 0
#define TWPC_STATE_SEND 1

#define TWPC_DATA_BITS ( sizeof(twpc_packet_t) * 8 )
#define TWPC_BUFFER_SIZE 32

typedef union {
	uint32_t data_raw;
	struct {
		uint8_t uid;
		uint8_t cmd;
		uint16_t arg;
	} __attribute__((packed));
} __attribute__((packed)) twpc_packet_t;

typedef struct {
	twpc_packet_t buffer[TWPC_BUFFER_SIZE];
	int start;
	int end;
} twpc_buffer_t;

static volatile twpc_buffer_t twpc_buffer_in;
static volatile twpc_buffer_t twpc_buffer_out;

static volatile int twpc_even = 1;

static volatile int twpc_bit = 0;
static volatile int twpc_data = 0;
static volatile int twpc_received = 0;

static volatile int twpc_state = 0;

static volatile int onewire_bit = 0;
static volatile int onewire_even = 0;

// inverted logic
static int twpc_read(void) {
	TWPC_DDR &= ~_BV(TWPC_P);
	return !( TWPC_PIN & _BV(TWPC_P) ) ? 1 : 0;
}

static void twpc_line_off(void) {
	TWPC_DDR |= _BV(TWPC_P);
	TWPC_PORT |= _BV(TWPC_P);
}

static void twpc_line_on(void) {
	TWPC_DDR |= _BV(TWPC_P);
	TWPC_PORT &= ~_BV(TWPC_P);
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

static void twpc_buffer_store(volatile twpc_buffer_t *b, twpc_packet_t *p) {
	int i = ( b->end + 1 ) % TWPC_BUFFER_SIZE;
	while ( b->start == i );
	memcpy(&(b->buffer[b->end]), p, sizeof(twpc_packet_t));
	b->end = i;
}

static twpc_packet_t twpc_buffer_get(volatile twpc_buffer_t *b) {
	if ( b->start != b->end ) {
		int t = b->start;
		b->start = ( b->start + 1 ) % TWPC_BUFFER_SIZE;
		return b->buffer[t];
	}
	return (twpc_packet_t)0;
}

static int twpc_buffer_available(volatile twpc_buffer_t *b) {
	return ( TWPC_BUFFER_SIZE + b->end - b->start ) % TWPC_BUFFER_SIZE;
}

void com_init(void) {
	OCR1A = 30; // 0.5ms
	TCCR1A = 0;
	TCCR1B = _BV(WGM12) | _BV(CS12); // F_CPU/256, CTC mode
	TIMSK1 = _BV(OCIE1A);
}

void com_send(int data) {
	twpc_bit = 0;
	twpc_data = data;
	twpc_state = TWPC_STATE_SEND;
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
	if ( !twpc_even ) {
		// code for sending ( synchronize )
		if ( twpc_state == TWPC_STATE_SEND && twpc_bit == 0 && twpc_read() ) {
			twpc_bit = 1;
			twpc_even = 1; // turn parity
		}
		// code for receiving
		if ( twpc_state == TWPC_STATE_RECV ) {
			if ( twpc_bit == 0 ) { // first start bit
				if ( twpc_read() ) {
					twpc_bit = 1;
				}
			} else if ( twpc_bit == 1 ) { // second start bit
				if ( !twpc_read() ) {
					twpc_bit = 2;
					twpc_data = 0;
				} else {
					twpc_bit = 0; // failed
				}
			} else if ( twpc_bit >= 2 && twpc_bit < 2 + TWPC_DATA_BITS ) { // read data
				twpc_data |= twpc_read() << ( twpc_bit - 2 );
				twpc_bit++;
			} else {
				twpc_bit = 0;
				twpc_received = 1;
			}
		}
	} else {
		// code for receiving ( synchronize )
		if ( twpc_state == TWPC_STATE_RECV && twpc_bit == 1 && !twpc_read() ) {
			twpc_bit = 2;
			twpc_data = 0;
			twpc_even = 0; // turn parity
		}
		// code for sending
		if ( twpc_state == TWPC_STATE_SEND ) {
			if ( twpc_bit == 0 ) { // waiting for first start bit
				if ( twpc_read() ) {
					twpc_bit = 1;
				}
			} else if ( twpc_bit == 1 ) {
				twpc_line_off(); // sending second start bit: '0' -> +V
				twpc_bit = 2;
			} else if ( twpc_bit >= 2 && twpc_bit < 2 + TWPC_DATA_BITS ) { // sending data
				if ( twpc_data & _BV(twpc_bit - 2) ) {
					twpc_line_on();
				} else {
					twpc_line_off();
				}
				twpc_bit++;
			} else {
				twpc_state = TWPC_STATE_RECV;
			}
		}
	}
	twpc_even ^= 1;
	onewire_even ^= 1;
}

void dbg(int t) {
	PORTB |= _BV(4);
	_delay_ms(t);
	PORTB &= ~_BV(4);
}

int main(void) {
	cli();
	led_init();
	com_init();
	DDRB |= _BV(4) | _BV(3); // debug
	sei();
	while ( 1 ) {
		if ( twpc_received ) {
			/*
			if ( twpc_data == 199 ) {
				dbg(1);
				com_send(85);
			}
			*/
			if ( twpc_data == TWPC_UID ) {
				led_blink();
			}
			twpc_received = 0;
		}
	}
	return 1;
}
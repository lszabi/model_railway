#include <avr/io.h>
#include <avr/interrupt.h>
#include <avr/pgmspace.h>
#include <util/delay.h>
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

#define LED_P 7
#define LED_PORT PORTB
#define LED_DDR DDRB

#define ONEWIRE_DDR DDRC
#define ONEWIRE_PORT PORTC
#define ONEWIRE_PIN PINC

#define TWPC_POWER_P 6
#define TWPC_POWER_DDR DDRB
#define TWPC_POWER_PORT PORTB

#define TWPC_DATA_P 5
#define TWPC_DATA_DDR DDRE
#define TWPC_DATA_PIN PINE

#define ONEWIRE_PINS_N 2

static const int onewire_pins[] = { 0, 1 }; // multiple places

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

// UART

/*
Interrupt-driven uart to communicate with raspberry pi
*/

#define SERIAL_BUF_SIZE 256

typedef struct {
	char buffer[SERIAL_BUF_SIZE];
	int start;
	int end;
} serial_ring_buffer_t;

volatile static serial_ring_buffer_t tx;
volatile static serial_ring_buffer_t rx;

static void buf_store(char c, volatile serial_ring_buffer_t *b) {
	int i = ( b->end + 1 ) % SERIAL_BUF_SIZE;
	while ( b->start == i );
	b->buffer[b->end] = c;
	b->end = i;
}

static char buf_get(volatile serial_ring_buffer_t *b) {
	if ( b->start != b->end ) {
		char c = b->buffer[b->start];
		b->start = ( b->start + 1 ) % SERIAL_BUF_SIZE;
		return c;
	}
	return 0;
}

static int buf_available(volatile serial_ring_buffer_t *b) {
	return ( SERIAL_BUF_SIZE + b->end - b->start ) % SERIAL_BUF_SIZE;
}

ISR(USART0_RX_vect) {
	buf_store(UDR0, &rx);
}

ISR(USART0_UDRE_vect) {
	if ( buf_available(&tx) ) {
		UDR0 = buf_get(&tx);
	} else {
		UCSR0B &= ~_BV(UDRIE1);
	}
}

void serial_flush(void) {
	while ( tx.start != tx.end );
	tx.start = 0;
	tx.end = 0;
	rx.start = 0;
	rx.end = 0;
}

void serial_flush_rx(void) {
	rx.start = 0;
	rx.end = 0;
}

void serial_init(uint16_t baud) {
	serial_flush();
	uint16_t b = ( ( F_CPU / ( baud * 8UL ) ) - 1 );
	UBRR0H = b >> 8;
	UBRR0L = b;
	UCSR0A |= ( 1 << U2X0 );
	UCSR0B |= (1 << RXEN0 ) | (1 << TXEN0 ) | ( 1 << RXCIE0 );
	UCSR0C |= (1 << UCSZ00) | (1 << UCSZ01);
	sei();
}

void serial_end(void) {
	serial_flush();
	UCSR0B = 0;
}

int serial_available(void) {
	return buf_available(&rx);
}
	
void serial_put(char c) {
	buf_store(c, &tx);
	UCSR0B |= _BV(UDRIE0);
}

void serial_write(const char *c) {
	while ( *c ) {
		buf_store(*c++, &tx);
	}
	UCSR0B |= _BV(UDRIE0);
}

char serial_get(void) {
	return buf_get(&rx);
}

char serial_wait(void) {
	while ( !serial_available() );
	return serial_get();
}

int serial_read_max(char *s, int l) {
	int i = 0;
	while ( serial_available() && ( l == 0 || i < l ) ) {
		s[i++] = serial_get();
	}
	s[i] = '\0';
	return i;
}

int serial_read(char *s) {
	return serial_read_max(s, 0);
}

char serial_peek(void) {
	if ( rx.end != rx.start ) {
		return rx.buffer[rx.start];
	}
	return 0;
}

// Communication

#define TWPC_STATE_IDLE 0
#define TWPC_STATE_SEND 1
#define TWPC_STATE_RECV 2

static volatile int twpc_even = 1;
static volatile int twpc_bit = 0;
static volatile int twpc_state = 0;

static volatile int onewire_even = 0;
static volatile int onewire_idx = 0;
static volatile int onewire_bit = 0;
static volatile int onewire_data = 0;

static volatile int onewire_pin = 0;
static volatile int onewire_dev = 0;
static volatile int onewire_got = 0;

static volatile int onewire_last[ONEWIRE_PINS_N];

static volatile twpc_packet_t twpc_data_send;
static volatile int twpc_sent = 1;

volatile twpc_packet_t twpc_data_recv;
volatile int twpc_received = 0;

// using inverted logic: '0' = +Vcc, '1' = 0V
static void twpc_line_on(void) {
	TWPC_POWER_PORT |= _BV(TWPC_POWER_P);
}

static void twpc_line_off(void) {
	TWPC_POWER_PORT &= ~_BV(TWPC_POWER_P);
}

static uint32_t twpc_read(void) {
	return !( TWPC_DATA_PIN & _BV(TWPC_DATA_P) ) ? 1UL : 0UL;
}

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

void com_init(void) {
	OCR1A = 30; // 0.5ms
	TCCR1A = 0;
	TCCR1B = _BV(WGM12) | _BV(CS12); // F_CPU/256, CTC mode
	TIMSK1 = _BV(OCIE1A);
	TWPC_POWER_DDR |= _BV(TWPC_POWER_P);
	TWPC_DATA_DDR &= ~_BV(TWPC_DATA_P);
	for ( int i = 0; i < ONEWIRE_PINS_N; i++ ) {
		onewire_last[i] = 0;
	}
}

void com_send(uint8_t uid, uint8_t cmd, uint16_t arg) {
	while ( !twpc_sent );
	twpc_sent = 0;
	twpc_data_send.uid = uid;
	twpc_data_send.cmd = cmd;
	twpc_data_send.arg = arg;
	twpc_bit = 0;
	twpc_state = TWPC_STATE_SEND;
}

/*
int com_recv(void) {
	twpc_bit = 0;
	twpc_received = 0;
	twpc_state = TWPC_STATE_RECV;
	while ( twpc_received == 0 );
	return twpc_data;
}
*/

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
			} else {
				twpc_line_off();
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
			} else if ( twpc_bit == 1 ) { // waiting for second start bit
				if ( !twpc_read() ) {
					twpc_data_recv.data_raw = 0;
					twpc_bit = 2;
				}
			} else if ( twpc_bit >= 2 && twpc_bit < 2 + TWPC_DATA_BITS ) { // read data
				twpc_data_recv.data_raw |= twpc_read() << ( twpc_bit - 2 );
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

int main(void) {
	cli();
	led_init();
	com_init();
	serial_init(57600);
	sei();
	while ( 1 ) {
		char c = serial_get();
		if ( c == '1' ) {
			com_send(255, TWPC_CMD_LIGHT_ON, 0);
		} else if ( c == '0' ) {
			com_send(255, TWPC_CMD_LIGHT_OFF, 0);
		} else if ( c == 'm' ) {
			c = serial_wait();
			int dir = read_byte('0', c);
			c = serial_wait();
			char c2 = serial_wait();
			int speed = read_byte(c, c2);
			com_send(255, TWPC_CMD_MOTOR, dir << 8 | speed);
		}
		if ( onewire_got ) {
			onewire_got = 0;
			char buf[10];
			write_byte(buf, onewire_pin);
			write_byte(buf + 2, onewire_dev);
			buf[4] = '\0';
			serial_write(buf);
		}
	}
	return 1;
}

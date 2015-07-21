#include <avr/io.h>
#include <avr/interrupt.h>
#include <avr/pgmspace.h>
#include <util/delay.h>

/*
1-wire communication
Master code
created by LSzabi

Receive Signal -> send signal + 8bit id

2 wire power communication (TWPC)
Master code

Generate power signal + manage slave devices

This is an "all-in-one" code, kinda messy...
*/

#define LED_P 7
#define LED_PORT PORTB
#define LED_DDR DDRB

#define ONEWIRE_DDR DDRD
#define ONEWIRE_PORT PORTD
#define ONEWIRE_PIN PIND

#define TWPC_POWER_P 6
#define TWPC_POWER_DDR DDRB
#define TWPC_POWER_PORT PORTB

#define TWPC_DATA_P 4
#define TWPC_DATA_DDR DDRE
#define TWPC_DATA_PORT PORTE
#define TWPC_DATA_PIN PINE

static const int onewire_pins[] = { 0, 1 }; // multiple places

static const int initial_devices[] = { 165, 0 }; // initially connected debice IDs (must end with zero)

// Some funny debug stuff
void blink_d(uint8_t d) {
	for ( int i = 0; i < 8; i++ ) {
		if ( d & ( 1 << i ) ) {
			PORTB |= _BV(0);
			_delay_ms(20);
			PORTB &= ~_BV(0);
			_delay_ms(400);
		} else {
			PORTB |= _BV(1);
			_delay_ms(20);
			PORTB &= ~_BV(1);
			_delay_ms(400);
		}
	}
}

void dbg(void) { // for logic analyzers
	PORTB |= _BV(0);
	_delay_us(2);
	PORTB &= ~_BV(0);
}

void led_on(void) {
	LED_PORT |= _BV(LED_P);
}

void led_off(void) {
	LED_PORT &= ~_BV(LED_P);
}

void blink(void) {
	led_on();
	_delay_ms(5);
	led_off();
}

// ** UART ** //
#if 1

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


ISR(USART1_RX_vect) {
	buf_store(UDR1, &rx);
}

ISR(USART1_UDRE_vect) {
	if ( buf_available(&tx) ) {
		UDR1 = buf_get(&tx);
	} else {
		UCSR1B &= ~_BV(UDRIE1);
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
	UBRR1H = b >> 8;
	UBRR1L = b;
	UCSR1A |= ( 1 << U2X1 );
	UCSR1B |= (1 << RXEN1 ) | (1 << TXEN1 ) | ( 1 << RXCIE1 );
	UCSR1C |= (1 << UCSZ10) | (1 << UCSZ11);
	sei();
}

void serial_end(void) {
	serial_flush();
	UCSR1B = 0;
}

int serial_available(void) {
	return buf_available(&rx);
}
	
void serial_put(char c) {
	buf_store(c, &tx);
	UCSR1B |= _BV(UDRIE1);
}

void serial_write(const char *c) {
	while ( *c ) {
		buf_store(*c++, &tx);
	}
	UCSR1B |= _BV(UDRIE1);
}

/*
void serial_write_p(const char *p) {
	register char c;
	while ( ( c = pgm_read_byte(p++) ) ) { 
		buf_store(c, &tx);
	}
	UCSR1B |= _BV(UDRIE1);
}
*/

char serial_get(void) {
	return buf_get(&rx);
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
#endif

// ** ONEWIRE ** //
#if 1

void onewire_wait_signal(int p) {
	ONEWIRE_DDR &= ~_BV(onewire_pins[p]);
	ONEWIRE_PORT &= ~_BV(onewire_pins[p]);
}

void onewire_send_signal(int p) {
	ONEWIRE_DDR |= _BV(onewire_pins[p]);
	ONEWIRE_PORT |= _BV(onewire_pins[p]);
	_delay_us(20);
	ONEWIRE_PORT &= ~_BV(onewire_pins[p]);
}

uint8_t onewire_receive_data(int p) {
	uint8_t d = 0;
	_delay_us(5);
	for ( int i = 0; i < 8; i++ ) {
		if ( ONEWIRE_PIN & _BV(onewire_pins[p]) ) {
			d |= _BV(i);
		}
		_delay_us(15);
	}
	return d;
}

int onewire_cycle(int p) {
	onewire_send_signal(p);
	onewire_wait_signal(p);
	_delay_us(10);
	if ( ONEWIRE_PIN & _BV(onewire_pins[p]) ) {
		while ( ONEWIRE_PIN & _BV(onewire_pins[p]) );
		return onewire_receive_data(p);
	}
	return -1;
}
#endif

// ** TWPC ** //
#if 1

#define TWPC_CMD_NOP 0x01
#define TWPC_CMD_END 0x02
#define TWPC_CMD_DATA 0x03
#define TWPC_CMD_ERR 0x04
#define TWPC_CMD_OK 0x05
#define TWPC_CMD_NEW_DEVICE 0x06
#define TWPC_CMD_DISCONNECT 0x07
#define TWPC_CMD_LIGHT_ON 0x08
#define TWPC_CMD_LIGHT_OFF 0x09
#define TWPC_CMD_STOP 0x0A
#define TWPC_CMD_MOTORA 0x0B
#define TWPC_CMD_MOTORB 0x0C
#define TWPC_CMD_UID1 0x0D
#define TWPC_CMD_UID2 0x0E
#define TWPC_CMD_UID3 0x0F
#define TWPC_CMD_TYPE 0x10

#define TWPC_DEV_STATION 0xFE
#define TWPC_DEV_TRAIN 0xFF

typedef struct {
	uint8_t type; // 0: disconnected, 1: train, 2: station, 3: not specified
	char uid[3];
	uint8_t send_data;
	int wait_data;
} device_t;

static int twpc_state = 0, twpc_connect_device = 1;
static device_t twpc_device_table[254] = { { 0, { 0, }, 0 }, }; // devices from 1 to 254
static uint8_t twpc_current_id = 1; // the next id in the queue
static uint8_t twpc_broadcast_data = 0;

// Master cycle
int twpc_cycle(int send_data) {
	int d = 0;
	// power cycle
	_delay_us(400);
	cli();
	TWPC_POWER_PORT |= _BV(TWPC_POWER_P);
	_delay_us(20);
	// data cycle
	if ( send_data ) {
		TWPC_POWER_PORT &= ~_BV(TWPC_POWER_P);
	}
	_delay_us(40);
	if ( !send_data && ( TWPC_DATA_PIN & _BV(TWPC_DATA_P) ) ) { // recevie signal
		d = 1;
	}
	_delay_us(40);
	TWPC_POWER_PORT |= _BV(TWPC_POWER_P);
	sei();
	_delay_us(20);
	TWPC_POWER_PORT &= ~_BV(TWPC_POWER_P);
	return d;
}

void twpc_init(void) {
#ifdef TWPC_POWER_DDR
	TWPC_POWER_DDR |= _BV(TWPC_POWER_P);
	TWPC_POWER_PORT &= ~_BV(TWPC_POWER_P);
#endif
	TWPC_DATA_DDR &= ~_BV(TWPC_DATA_P);
	TWPC_DATA_PORT &= ~_BV(TWPC_DATA_P);
}

void twpc_send_data(uint8_t d) {
	for ( int i = 0; i < 8; i++ ) {
		twpc_cycle(d & _BV(i));
	}
}

uint8_t twpc_receive_data(void) {
	uint8_t d = 0;
	for ( int i = 0; i < 8; i++ ) {
		if ( twpc_cycle(0) ) {
			d |= _BV(i);
		}
	}
	return d;
}

void twpc_next_id(void) {
	do {
		twpc_current_id++;
		if ( twpc_current_id > 254 ) {
			twpc_current_id = 1;
		}
	} while ( !( twpc_device_table[twpc_current_id - 1].type ) );
}

int twpc_dev_rem(void) {
	for ( int i = 0; i < 253; i++ ) {
		if ( twpc_device_table[i].type ) {
			return 1;
		}
	}
	return 0;
}
 
#endif
 
// ** main ** //

int hextoint(char *s) {
	int ret = 0;
	while ( *s ) {
		ret <<= 4;
		if ( *s <= 'F' && *s >= 'A' ) {
			ret |= *s - 'A' + 10;
		} else if ( *s <= '9' && *s >= '0' ) {
			ret |= *s - '0';
		}
		s++;
	}
	return ret;
}

// convert to fixed 8 char string
void inttohex(char *str, uint32_t n) {
	int l = 0;
	for ( int i = 28; i >= 0; i -= 4 ) {
		int x = ( n >> i ) & 0x0F;
		if ( x > 9 ) {
			str[l] = x + 'A';
		} else {
			str[l] = x + '0';
		}
		l++;
	}
	str[l] = '\0';
}

static uint32_t con_log = 0;

int main(void) {
	LED_DDR |= _BV(LED_P);
	led_off();
	PORTE &= ~_BV(0); // UART RX pin
	DDRE &= ~_BV(0);
	twpc_init();
	serial_init(57600);
	int count_init = 0;
	for ( count_init = 0; initial_devices[count_init]; count_init++ ) {
		int id = initial_devices[count_init] - 1;
		twpc_device_table[id].type = 3;
		twpc_device_table[id].send_data = TWPC_CMD_TYPE;
		twpc_device_table[id].wait_data = 2;
	}
	if ( count_init > 0 ) {
		twpc_next_id();
		twpc_state = 2;
		twpc_connect_device = 0;
	}
	_delay_ms(2000);
	blink();
	while ( 1 ) {
		if ( twpc_state == 0 ) { // waiting for 1 bit (connect devices)
			led_on();
			if ( !twpc_connect_device ) { // connection cancelled, send dummy datas
				led_off();
				twpc_cycle(1);
				twpc_send_data(0xFF);
				twpc_send_data(0xFF);
				twpc_send_data(0xFF);
				twpc_state = 2;
			} else if ( twpc_cycle(0) ) {
				led_off();
				int new_id;
				for ( new_id = 0; new_id < 254; new_id++ ) {
					if ( twpc_device_table[new_id].type == 0 ) {
						new_id++;
						break;
					}
				}
				twpc_send_data(new_id); // send next available id to new device
				twpc_current_id = new_id;
				twpc_device_table[twpc_current_id - 1].send_data = 0;
				twpc_device_table[twpc_current_id - 1].wait_data = 2;
				twpc_state = 1;
				twpc_connect_device = 0;
			}
		} else if ( twpc_state == 1 ) { // waiting for data + END
			uint8_t data = twpc_receive_data();
			uint8_t end = twpc_receive_data();
			int dc = 0;
			if ( twpc_device_table[twpc_current_id - 1].wait_data == 0 ) { // nothing
				if ( data != TWPC_CMD_OK ) {
					// disonnect
					dc = 1;
					// save the received packets for the sake of debugging
					con_log = ( (uint32_t)data << 16 ) | ( (uint32_t)end << 8 ) | twpc_current_id;
				}
			} else if ( twpc_device_table[twpc_current_id - 1].wait_data == 1 ) { // anything
				if ( data == TWPC_CMD_DISCONNECT ) {
					dc = 1;
				} else if ( data == TWPC_CMD_NOP ) {
					// nothing happens here
				}
			} else if ( twpc_device_table[twpc_current_id - 1].wait_data == 2 ) { // device type
				if ( data == TWPC_DEV_TRAIN ) {
					twpc_device_table[twpc_current_id - 1].type = 1;
				} else if ( data == TWPC_DEV_STATION ) {
					twpc_device_table[twpc_current_id - 1].type = 2;
				} else {
					dc = 1;
					con_log = 0x1AB;
				}
				twpc_device_table[twpc_current_id - 1].send_data = TWPC_CMD_UID1;
				twpc_device_table[twpc_current_id - 1].wait_data = 3;
			} else if ( twpc_device_table[twpc_current_id - 1].wait_data == 3 ) {
				twpc_device_table[twpc_current_id - 1].uid[0] = data;
				twpc_device_table[twpc_current_id - 1].send_data = TWPC_CMD_UID2;
				twpc_device_table[twpc_current_id - 1].wait_data = 4;
			} else if ( twpc_device_table[twpc_current_id - 1].wait_data == 4 ) {
				twpc_device_table[twpc_current_id - 1].uid[1] = data;
				twpc_device_table[twpc_current_id - 1].send_data = TWPC_CMD_UID3;
				twpc_device_table[twpc_current_id - 1].wait_data = 5;
			} else if ( twpc_device_table[twpc_current_id - 1].wait_data == 5 ) {
				twpc_device_table[twpc_current_id - 1].uid[2] = data;
				twpc_device_table[twpc_current_id - 1].send_data = 0;
				twpc_device_table[twpc_current_id - 1].wait_data = 0;
			}
			if ( dc ) {
				twpc_device_table[twpc_current_id - 1].type = 0;
				if ( twpc_dev_rem() ) {
					twpc_next_id();
					twpc_state = 2;
				} else {
					twpc_connect_device = 1;
					twpc_state = 0;
				}
			} else {
				twpc_state = 2;
				if ( end == TWPC_CMD_END ) {
					twpc_next_id();
				}
			}
		} else if ( twpc_state == 2 ) { // sending data
			if ( twpc_broadcast_data ) { // broadcast data
				twpc_send_data(255);
				twpc_send_data(twpc_broadcast_data);
				twpc_broadcast_data = 0;
			} else if ( twpc_connect_device ) { // connecting new device, everybody chill out
				twpc_send_data(255);
				twpc_send_data(TWPC_CMD_NEW_DEVICE);
				twpc_state = 0;
			} else {
				twpc_send_data(twpc_current_id);
				if ( twpc_device_table[twpc_current_id - 1].send_data ) { // tell 'em what we want
					twpc_send_data(twpc_device_table[twpc_current_id - 1].send_data);
					twpc_device_table[twpc_current_id - 1].send_data = 0;
				} else {
					twpc_send_data(TWPC_CMD_NOP);
					twpc_device_table[twpc_current_id - 1].wait_data = 0;
				}
				twpc_state = 1;
			}
		}
		cli();
		int id = onewire_cycle(0);
		if ( id != -1 ) {
			blink();
			char str[3];
			inttohex(str, id);
			serial_write(str);
			serial_put(' ');
			twpc_device_table[id - 1].send_data = TWPC_CMD_LIGHT_ON; // turn on the lights on the contacted device (temporary solution)
		}
		id = onewire_cycle(1);
		if ( id != -1 ) {
			twpc_device_table[id - 1].send_data = TWPC_CMD_LIGHT_OFF;
		}
		sei();
		if ( serial_available() >= 4 ) {
			char id_s[4];
			serial_get(); // receiving garbage byte on connection, will be fixed (or not)
			serial_read_max(id_s, 3);
			char c = id_s[2];
			id_s[2] = '\0';
			int id = hextoint(id_s);
			if ( c == '1' ) { // lights on
				if ( id == 255 ) {
					twpc_broadcast_data = TWPC_CMD_LIGHT_ON;
				} else {
					twpc_device_table[id - 1].send_data = TWPC_CMD_LIGHT_ON;
				}
				serial_write("on");
			} else if ( c == '0' ) { // lights off
				if ( id == 255 ) {
					twpc_broadcast_data = TWPC_CMD_LIGHT_OFF;
				} else {
					twpc_device_table[id - 1].send_data = TWPC_CMD_LIGHT_OFF;
				}
				serial_write("off");
			} else if ( c == 'g' ) { // get list of connected devices
				char str[65] = { 0, };
				int s = 0;
				for ( int i = 0; i < 254; i += 32 ) {
					uint32_t n = 0;
					for ( int f = 0; f < 32 && i + f < 254; f++ ) {
						if ( twpc_device_table[i + f].type ) {
							n |= _BV(f);
						}
					}
					inttohex(str + s, n);
					s += 8;
					str[s++] = ',';
				}
				serial_write(str);
				_delay_ms(100);
			} else if ( c == 'c' ) { // connect new device
				twpc_connect_device = 1;
				serial_write("con");
			} else if ( c == 'r' ) { // cancel connecting new device
				twpc_connect_device = 0;
				serial_write("res");
			} else if ( c == 'l' ) { // get disconnect log
				char str[9];
				inttohex(str, con_log);
				serial_write(str);
			} else if ( c == 'i' ) {
				serial_put(twpc_device_table[id - 1].uid[0]);
				serial_put(twpc_device_table[id - 1].uid[1]);
				serial_put(twpc_device_table[id - 1].uid[2]);
			} else if ( c == 's' ) {
				if ( id == 255 ) {
					twpc_broadcast_data = TWPC_CMD_STOP;
				} else {
					twpc_device_table[id - 1].send_data = TWPC_CMD_STOP;
				}
				serial_write("stopped");
			} else if ( c == 'a' ) {
				if ( id == 255 ) {
					twpc_broadcast_data = TWPC_CMD_MOTORA;
				} else {
					twpc_device_table[id - 1].send_data = TWPC_CMD_MOTORA;
				}
				serial_write("dir a");
			} else if ( c == 'b' ) {
				if ( id == 255 ) {
					twpc_broadcast_data = TWPC_CMD_MOTORB;
				} else {
					twpc_device_table[id - 1].send_data = TWPC_CMD_MOTORB;
				}
				serial_write("dir b");
			}
			serial_flush_rx(); // get rid of remaining data, it could cause 'misunderstanding'
		}
		_delay_ms(1);
	}
	return 1;
}

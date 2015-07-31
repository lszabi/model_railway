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

#define TWPC_UID 0xAA

#define LED_P 0
#define LED_PORT PORTA
#define LED_DDR DDRA

#define TWPC_P 1
#define TWPC_PORT PORTB
#define TWPC_PIN PINB
#define TWPC_DDR DDRB

#define ONEWIRE_P 0
#define ONEWIRE_PORT PORTB
#define ONEWIRE_PIN PINB
#define ONEWIRE_DDR DDRB

// LED

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

// Motor

/*
Mode	PCH_A	PCH_B	PWM_A	PWM_B	func
off		1		1		1		1		motor_off()
pwm_a	0		1		0		pwm		motor_a(pwm)
pwm_b	1		0		pwm		0		motor_b(pwm)
*/

static void motor_pch_a(int s) {
	if ( s ) {
		PORTA |= _BV(7);
	} else {
		PORTA &= ~_BV(7);
	}
}

static void motor_pch_b(int s) {
	if ( s ) {
		PORTA |= _BV(4);
	} else {
		PORTA &= ~_BV(4);
	}
}

static void motor_pwm_a_dig(int s) {
	if ( s ) {
		PORTA |= _BV(6);
	} else {
		PORTA &= ~_BV(6);
	}
}

static void motor_pwm_b_dig(int s) {
	if ( s ) {
		PORTA |= _BV(5);
	} else {
		PORTA &= ~_BV(5);
	}
}

static void motor_pwm_a(uint8_t s) {
	PORTA &= ~_BV(6);
	TCCR1A = _BV(COM1A1) | _BV(WGM10);
	TCCR1B = _BV(CS12) | _BV(WGM12);
	OCR1AH = 0;
	OCR1AL = s;
}

static void motor_pwm_b(uint8_t s) {
	PORTA &= ~_BV(5);
	TCCR1A = _BV(COM1B1) | _BV(WGM10);
	TCCR1B = _BV(CS12) | _BV(WGM12);
	OCR1BH = 0;
	OCR1BL = s;
}

static void motor_detach_pwm(void) {
	TCCR1A = 0;
	TCCR1B = 0;
}

// Use only the funtions below to operate motor

void motor_off(void) {
	motor_detach_pwm();
	motor_pch_a(1);
	motor_pch_b(1);
	motor_pwm_a_dig(1);
	motor_pwm_b_dig(1);
}

void motor_init(void) {
	DDRA |= _BV(4) | _BV(5) | _BV(6) | _BV(7);
	motor_off();
}

void motor_a(uint8_t s) {
	motor_pch_a(0);
	motor_pch_b(1);
	motor_pwm_a_dig(0);
	if ( s == 0xFF ) {
		motor_detach_pwm();
		motor_pwm_b_dig(1);
	} else if ( s == 0 ) {
		motor_off();
	} else {
		motor_pwm_b(s);
	}
}

void motor_b(uint8_t s) {
	motor_pch_a(1);
	motor_pch_b(0);
	motor_pwm_b_dig(0);
	if ( s == 0xFF ) {
		motor_detach_pwm();
		motor_pwm_a_dig(1);
	} else if ( s == 0 ) {
		motor_off();
	} else {
		motor_pwm_a(s);
	}
}

// Communication


static volatile int twpc_even = 1;

static volatile int twpc_bit = 0;
static volatile int twpc_send = 0;

static volatile int onewire_bit = 0;
static volatile int onewire_even = 0;

volatile twpc_packet_t com_data;
volatile int com_received = 0;

// inverted logic
static uint32_t twpc_read(void) {
	TWPC_DDR &= ~_BV(TWPC_P);
	return !( TWPC_PIN & _BV(TWPC_P) ) ? 1UL : 0UL;
}

static void twpc_line_off(void) { // '0'
	TWPC_DDR |= _BV(TWPC_P);
	TWPC_PORT |= _BV(TWPC_P);
}

static void twpc_line_on(void) { // '1'
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

void com_init(void) {
	OCR0A = 62; // 0.5ms
	TCCR0A = _BV(WGM01); // CTC mode
	TCCR0B = _BV(CS01) | _BV(CS00); // F_CPU/64
	TIMSK0 = _BV(OCIE0A);
}

void com_send(void) {
	if ( twpc_bit == 0 ) {
		twpc_send = 1;
	}
}

ISR(TIM0_COMPA_vect) {
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
		if ( twpc_send ) {
			// code for sending
			if ( twpc_bit == 0 ) {
				if ( twpc_read() ) {
					twpc_bit = 1;
				}
			} else if ( twpc_bit == 1 ) { // sending 2nd start bit
				twpc_line_off();
				twpc_bit = 2;
			} else if ( twpc_bit >= 2 && twpc_bit < 2 + TWPC_DATA_BITS ) { // sending data
				if ( com_data.data_raw & ( 1UL << twpc_bit - 2 ) ) {
					twpc_line_on();
				} else {
					twpc_line_off();
				}
				twpc_bit++;
			} else if ( twpc_bit == 2 + TWPC_DATA_BITS ) {
				twpc_bit++;
			} else {
				twpc_send = 0;
				twpc_bit = 0;
			}
		} else {
			// code for receiving
			if ( twpc_bit == 0 ) { // first start bit
				if ( twpc_read() ) {
					twpc_bit = 1;
				}
			} else if ( twpc_bit == 1 ) { // second start bit
				if ( !twpc_read() ) {
					twpc_bit = 2;
					com_data.data_raw = 0;
				}
			} else if ( twpc_bit >= 2 && twpc_bit < 2 + TWPC_DATA_BITS ) { // read data
				com_data.data_raw |= twpc_read() << ( twpc_bit - 2 );
				twpc_bit++;
			} else if ( twpc_bit == 2 + TWPC_DATA_BITS ) {
				twpc_bit++;
			} else {
				twpc_bit = 0;
				com_received = 1;
			}
		}
	} else {
		if ( !twpc_send ) {
			// code for receiving ( synchronize )
			if ( twpc_bit == 0 && twpc_read() ) {
				twpc_bit = 1;
			} else if ( twpc_bit == 1 && !twpc_read() ) {
				twpc_bit = 2;
				com_data.data_raw = 0;
				twpc_even = 1; // turn parity
			}
		}
	}
	twpc_even ^= 1;
	onewire_even ^= 1;
}

// Main

int main(void) {
	cli();
	_delay_ms(200);
	led_init();
	com_init();
	motor_init();
	sei();
	led_blink();
	while ( 1 ) {
		if ( com_received ) {
			if ( com_data.checksum == TWPC_CHECKSUM(com_data) ) {
				if ( com_data.uid == TWPC_UID || com_data.uid == 255 ) {
					if ( com_data.cmd == TWPC_CMD_LIGHT_ON ) {
						led_on();
					} else if ( com_data.cmd == TWPC_CMD_LIGHT_OFF ) {
						led_off();
					} else if ( com_data.cmd == TWPC_CMD_MOTOR_A ) {				
						uint8_t speed = com_data.arg;
						motor_a(speed);
					} else if ( com_data.cmd == TWPC_CMD_MOTOR_B ) {				
						uint8_t speed = com_data.arg;
						motor_b(speed);
					}
				}
				if ( com_data.uid == TWPC_UID ) {
					/*
					_delay_ms(100);
					com_data.cmd = TWPC_CMD_ACK;
					com_data.arg = 0;
					com_data.checksum = TWPC_CHECKSUM(com_data);
					*/
					com_received = 0;
					//com_send();
				}
			}
			com_received = 0;
		}
	}
	return 1;
}
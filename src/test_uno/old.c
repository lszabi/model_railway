#include <avr/io.h>
#include <avr/interrupt.h>
#include <util/delay.h>

/*
1-wire communication
Slave code
created by LSzabi

Send signal -> Receive Signal + 8bit id

2 wire power communication (TWPC)
Slave code

This code will run the brains of the train engines.
*/

#define LED_P 0
#define LED_PORT PORTA
#define LED_DDR DDRA

#define ONEWIRE_P 0
#define ONEWIRE_DDR DDRB
#define ONEWIRE_PORT PORTB
#define ONEWIRE_PIN PINB

#define TWPC_DATA_P 1
#define TWPC_DATA_DDR DDRB
#define TWPC_DATA_PORT PORTB
#define TWPC_DATA_PIN PINB

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

static const char twpc_uid[] = "ASD";

static const int twpc_static_id = 165; // set 0 to disable

static int twpc_state = 0;
static uint8_t twpc_my_id = 0; // my own id
static uint8_t twpc_response = 0; // response to send

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

void dbg(void) {
	led_on();
	_delay_us(2);
	led_off();
}

// ** MOTOR ** //

/*
Mode	PCH_A	PCH_B	PWM_A	PWM_B	func
off		1		1		1		1		motor_off()
pwm_a	0		1		0		pwm		motor_a(pwm)
pwm_b	1		0		pwm		0		motor_b(pwm)
*/

void motor_pch_a(int s) {
	if ( s ) {
		PORTA |= _BV(7);
	} else {
		PORTA &= ~_BV(7);
	}
}

void motor_pch_b(int s) {
	if ( s ) {
		PORTA |= _BV(4);
	} else {
		PORTA &= ~_BV(4);
	}
}

void motor_pwm_a_dig(int s) {
	if ( s ) {
		PORTA |= _BV(6);
	} else {
		PORTA &= ~_BV(6);
	}
}

void motor_pwm_b_dig(int s) {
	if ( s ) {
		PORTA |= _BV(5);
	} else {
		PORTA &= ~_BV(5);
	}
}

void motor_pwm_a(uint8_t s) {
	PORTA &= ~_BV(6);
	TCCR1A = _BV(COM1A1) | _BV(WGM10);
	TCCR1B = _BV(CS12) | _BV(WGM12);
	OCR1AH = 0;
	OCR1AL = s;
}

void motor_pwm_b(uint8_t s) {
	PORTA &= ~_BV(5);
	TCCR1A = _BV(COM1B1) | _BV(WGM10);
	TCCR1B = _BV(CS12) | _BV(WGM12);
	OCR1BH = 0;
	OCR1BL = s;
}

void motor_detach_pwm(void) {
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
	DDRB |= _BV(0);
	DDRA |= _BV(0) | _BV(5) | _BV(6);
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

// ** ONEWIRE ** //

void onewire_wait_signal(void) {
	ONEWIRE_DDR &= ~_BV(ONEWIRE_P);
	ONEWIRE_PORT &= ~_BV(ONEWIRE_P);
}

void onewire_send_signal(void) {
	ONEWIRE_DDR |= _BV(ONEWIRE_P);
	ONEWIRE_PORT |= _BV(ONEWIRE_P);
	_delay_us(10);
	ONEWIRE_PORT &= ~_BV(ONEWIRE_P);
}

void onewire_send_data(uint8_t d) {
	for ( int i = 0; i < 8; i++ ) {
		if ( d & _BV(i) ) {
			onewire_send_signal();
		} else {
			_delay_us(10);
		}
		_delay_us(5);
	}
}

void onewire_cycle(void) {
	if ( ONEWIRE_PIN & _BV(ONEWIRE_P) ) {
		while ( ONEWIRE_PIN & _BV(ONEWIRE_P) );
		// response
		_delay_us(5);
		onewire_send_signal();
		onewire_send_data(twpc_my_id);
		onewire_wait_signal();
	}
}

// ** TWPC ** //

void twpc_init(void) {
#ifdef TWPC_POWER_DDR
	TWPC_POWER_DDR |= _BV(TWPC_POWER_P);
	TWPC_POWER_PORT &= ~_BV(TWPC_POWER_P);
#endif
	TWPC_DATA_DDR &= ~_BV(TWPC_DATA_P);
	TWPC_DATA_PORT &= ~_BV(TWPC_DATA_P);
}

// Slave cycle
int twpc_cycle(int send_data) {
	int d = 0;
	// power cycle
	while ( !( TWPC_DATA_PIN & _BV(TWPC_DATA_P) ) );
	while ( TWPC_DATA_PIN & _BV(TWPC_DATA_P) ) {
		onewire_cycle(); // listen to onewire while we are in idle cycle
	}
	_delay_us(20);
	// data cycle
	if ( send_data ) {
		TWPC_DATA_DDR |= _BV(TWPC_DATA_P);
		TWPC_DATA_PORT |= _BV(TWPC_DATA_P);
	}
	_delay_us(40);
	if ( !send_data ) {
		TWPC_DATA_DDR &= ~_BV(TWPC_DATA_P);
		TWPC_DATA_PORT &= ~_BV(TWPC_DATA_P);
		if ( TWPC_DATA_PIN & _BV(TWPC_DATA_P) ) {
			d = 1;
		}
	}
	_delay_us(40);
	TWPC_DATA_DDR &= ~_BV(TWPC_DATA_P);
	TWPC_DATA_PORT &= ~_BV(TWPC_DATA_P);
	return d;
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

int main(void) {
	LED_DDR |= _BV(LED_P);
	led_off();
	twpc_init();
	motor_init();
	onewire_wait_signal();
	_delay_ms(1000); // wait for proper connection and power
	if ( twpc_static_id ) {
		twpc_my_id = twpc_static_id;
		twpc_state = 2;
	}
	blink();
	while ( 1 ) {
		if ( twpc_state == 0 ) { // connecting to master ( could've been before the while(1), but it isn't )
			twpc_cycle(1);
			twpc_my_id = twpc_receive_data();
			twpc_state = 1;
			twpc_response = TWPC_DEV_TRAIN;
		} else if ( twpc_state == 1 ) { // send response to the master
			twpc_send_data(twpc_response);
			twpc_send_data(TWPC_CMD_END);
			twpc_state = 2;
		} else if ( twpc_state == 2 ) { // receive data from our great lord and master
			uint8_t id = twpc_receive_data();
			uint8_t cmd = twpc_receive_data();
			if ( id == twpc_my_id || id == 255 ) {
				if ( cmd == TWPC_CMD_NOP ) { // obey the mighty mega
					twpc_response = TWPC_CMD_OK;
				} else if ( cmd == TWPC_CMD_LIGHT_ON ) {
					led_on();
					twpc_response = TWPC_CMD_OK;
				} else if ( cmd == TWPC_CMD_LIGHT_OFF ) {
					led_off();
					twpc_response = TWPC_CMD_OK;
				} else if ( cmd == TWPC_CMD_STOP ) {
					motor_off();
					twpc_response = TWPC_CMD_OK;
				} else if ( cmd == TWPC_CMD_MOTORA ) {
					motor_a(40);
					twpc_response = TWPC_CMD_OK;
				} else if ( cmd == TWPC_CMD_MOTORB ) {
					motor_b(40);
					twpc_response = TWPC_CMD_OK;
				} else if ( cmd == TWPC_CMD_NEW_DEVICE ) {
					twpc_state = 3;
				} else if ( cmd == TWPC_CMD_UID1 ) {
					twpc_response = twpc_uid[0];
				} else if ( cmd == TWPC_CMD_UID2 ) {
					twpc_response = twpc_uid[1];
				} else if ( cmd == TWPC_CMD_UID3 ) {
					twpc_response = twpc_uid[2];
				} else if ( cmd == TWPC_CMD_TYPE ) {
					twpc_response = TWPC_DEV_TRAIN;
				}
			}
			if ( id == twpc_my_id ) { // don't send response to broadcast packet
				twpc_state = 1;
			}
		} else if ( twpc_state == 3 ) { // wait until the new guy connects
			if ( twpc_cycle(0) ) {
				twpc_receive_data();
				twpc_receive_data();
				twpc_receive_data();
				twpc_state = 2;
			}
		}
	}
	return 1; // just why? i can't even...
}

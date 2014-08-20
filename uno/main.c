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

The same as the ATtiny84A code
(except for pin definition macros below)
see ../tiny84a/main.c
*/

#define LED_P 5
#define LED_PORT PORTB
#define LED_DDR DDRB

#define ONEWIRE_P 2
#define ONEWIRE_DDR DDRD
#define ONEWIRE_PORT PORTD
#define ONEWIRE_PIN PIND

#define TWPC_DATA_P 3
#define TWPC_DATA_DDR DDRD
#define TWPC_DATA_PORT PORTD
#define TWPC_DATA_PIN PIND

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
	PORTB |= _BV(0);
	_delay_us(2);
	PORTB &= ~_BV(0);
}

// ** ONEWIRE ** //

void onewire_wait_signal(void) {
	ONEWIRE_DDR &= ~_BV(ONEWIRE_P);
	ONEWIRE_PORT &= ~_BV(ONEWIRE_P);
}

void onewire_send_signal(void) {
	ONEWIRE_DDR |= _BV(ONEWIRE_P);
	ONEWIRE_PORT |= _BV(ONEWIRE_P);
	_delay_us(20);
	ONEWIRE_PORT &= ~_BV(ONEWIRE_P);
}

void onewire_send_data(uint8_t d) {
	for ( int i = 0; i < 8; i++ ) {
		if ( d & _BV(i) ) {
			onewire_send_signal();
		} else {
			_delay_us(20);
		}
	}
}

void onewire_cycle(void) {
	if ( ONEWIRE_PIN & _BV(ONEWIRE_P) ) {
		while ( ONEWIRE_PIN & _BV(ONEWIRE_P) );
		//dbg();
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
		onewire_cycle();
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
	onewire_wait_signal();
	_delay_ms(2000); // wait for proper connection and power
	blink();
	while ( !( TWPC_DATA_PIN & _BV(TWPC_DATA_P) ) );
	while ( TWPC_DATA_PIN & _BV(TWPC_DATA_P) );
	while ( 1 ) {
		/* */
		if ( twpc_state == 0 ) {
			twpc_cycle(1);
			twpc_my_id = twpc_receive_data();
			twpc_state = 1;
			twpc_response = TWPC_CMD_OK;
		} else if ( twpc_state == 1 ) {
			twpc_send_data(twpc_response);
			twpc_send_data(TWPC_CMD_END);
			twpc_state = 2;
		} else if ( twpc_state == 2 ) {
			uint8_t id = twpc_receive_data();
			uint8_t cmd = twpc_receive_data();
			if ( id == twpc_my_id || id == 255 ) {
				if ( cmd == TWPC_CMD_NOP ) {
					twpc_response = TWPC_CMD_OK;
				} else if ( cmd == TWPC_CMD_LIGHT_ON ) {
					led_on();
					twpc_response = TWPC_CMD_OK;
				} else if ( cmd == TWPC_CMD_LIGHT_OFF ) {
					led_off();
					twpc_response = TWPC_CMD_OK;
				} else if ( cmd == TWPC_CMD_NEW_DEVICE ) {
					twpc_state = 3;
				}
			}
			if ( id == twpc_my_id ) { // don't send response to broadcast packet
				twpc_state = 1;
			}
		} else if ( twpc_state == 3 ) {
			if ( twpc_cycle(0) ) {
				twpc_receive_data();
				twpc_receive_data();
				twpc_receive_data();
				twpc_state = 2;
			}
		}
		/* */
	}
	return 1;
}

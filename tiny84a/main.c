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

#define LED_P 3
#define LED_PORT PORTA
#define LED_DDR DDRA

#define ONEWIRE_P 2
#define ONEWIRE_DDR DDRB
#define ONEWIRE_PORT PORTB
#define ONEWIRE_PIN PINB

#define TWPC_DATA_P 7
#define TWPC_DATA_DDR DDRA
#define TWPC_DATA_PORT PORTA
#define TWPC_DATA_PIN PINA

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
	led_on();
	_delay_us(2);
	led_off();
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
	onewire_wait_signal();
	_delay_ms(2000); // wait for proper connection and power
	blink();
	while ( !( TWPC_DATA_PIN & _BV(TWPC_DATA_P) ) );
	while ( TWPC_DATA_PIN & _BV(TWPC_DATA_P) ); // wait for the end of a full idle cycle
	while ( 1 ) {
		if ( twpc_state == 0 ) { // connecting to master ( could've been before the while(1), but it isn't )
			twpc_cycle(1);
			twpc_my_id = twpc_receive_data();
			twpc_state = 1;
			twpc_response = TWPC_CMD_OK;
		} else if ( twpc_state == 1 ) { // send response to the master
			twpc_send_data(twpc_response);
			twpc_send_data(TWPC_CMD_END);
			twpc_state = 2;
		} else if ( twpc_state == 2 ) { // receive data from our great lord and master
			uint8_t id = twpc_receive_data();
			uint8_t cmd = twpc_receive_data();
			if ( id == twpc_my_id || id == 255 ) {
				if ( cmd == TWPC_CMD_NOP ) { // obey the mighty mega2560
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

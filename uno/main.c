#include <avr/io.h>
#include <avr/interrupt.h>
#include <util/delay.h>

/*
1-wire communication
Slave code

Send signal -> Receive Signal + 8bit id

2 wire power communication (TWPC)
Slave code

v0.7
*/

#define LED_P 5

#define ONEWIRE_P 2
#define ONEWIRE_DDR DDRD
#define ONEWIRE_PORT PORTD

#define TWPC_DATA_P 3
#define TWPC_DATA_DDR DDRD
#define TWPC_DATA_PORT PORTD
#define TWPC_DATA_PIN PIND

#define TWPC_DELAY() _delay_us(200);

volatile int onewire_signal_caught = 0;

volatile int twpc_signal_received = 0;

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
	PORTB |= _BV(LED_P);
}

void led_off(void) {
	PORTB &= ~_BV(LED_P);
}

void blink(void) {
	led_on();
	_delay_ms(5);
	led_off();
}

ISR(INT0_vect) {
	onewire_signal_caught = 1;
}

ISR(INT1_vect) {
	twpc_signal_received = 1;
}

int onewire_signal_received(void) {
	if ( onewire_signal_caught ) {
		onewire_signal_caught = 0;
		return 1;
	}
	return 0;
}

void onewire_wait_signal(void) {
	ONEWIRE_DDR &= ~_BV(ONEWIRE_P);
	ONEWIRE_PORT &= ~_BV(ONEWIRE_P);
}

void onewire_send_signal(void) {
	ONEWIRE_DDR |= _BV(ONEWIRE_P);
	ONEWIRE_PORT |= _BV(ONEWIRE_P);
	_delay_ms(1);
	ONEWIRE_PORT &= ~_BV(ONEWIRE_P);
}

void onewire_send_data(uint8_t d) {
	for ( int i = 0; i < 8; i++ ) {
		if ( d & _BV(i) ) {
			onewire_send_signal();
		} else {
			_delay_ms(1);
		}
	}
}

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
	// power cycle
	while ( !( TWPC_DATA_PIN & _BV(TWPC_DATA_P) ) );
	while ( TWPC_DATA_PIN & _BV(TWPC_DATA_P) );
	twpc_signal_received = 0;
	_delay_us(10);
	// data cycle
	if ( send_data ) {
		TWPC_DATA_DDR |= _BV(TWPC_DATA_P);
		TWPC_DATA_PORT |= _BV(TWPC_DATA_P);
	}
	TWPC_DELAY();
	TWPC_DATA_DDR &= ~_BV(TWPC_DATA_P);
	TWPC_DATA_PORT &= ~_BV(TWPC_DATA_P);
	if ( !send_data && twpc_signal_received ) {
		return 1;
	}
	return 0;
}

void twpc_send_data(uint8_t d) {
	twpc_cycle(1);
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
	cli();
	DDRD |= _BV(0);
	DDRB |= _BV(LED_P);
	PORTB &= ~_BV(LED_P);
	twpc_init();
	EICRA = _BV(ISC01) | _BV(ISC11) | _BV(ISC10);
	EIMSK = _BV(INT0) | _BV(INT1);
	sei();
	while ( 1 ) {
		/* * /
		if ( twpc_cycle(0) ) {
			if ( twpc_receive_data() == 0x32 ) {
				twpc_send_data(0x15);
				blink();
			}
		}
		// */
		/* */
		onewire_wait_signal();
		if ( onewire_signal_received() ) {
			// response
			onewire_send_signal();
			onewire_send_data(0x32);
			// clear signal flag
			onewire_signal_received();
			blink();
		}
		// */
	}
	return 1;
}

#include <avr/io.h>
#include <avr/interrupt.h>
#include <util/delay.h>

/*
1-wire communication
Master code

Send signal -> Receive Signal + 8bit id

2 wire power communication (TWPC)
Slave code
v0.5
*/

#define ONEWIRE_DATA_P 2
#define LED_P 5

#define TWPC_DATA_P 3

#define TWPC_DELAY() _delay_us(200);

volatile int onewire_signal_caught = 0;

volatile int _twpc_signal_received = 0;
int twpc_signal_received = 0;

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
	_twpc_signal_received = 1;
}

int onewire_signal_received(void) {
	if ( onewire_signal_caught ) {
		onewire_signal_caught = 0;
		return 1;
	}
	return 0;
}

void onewire_wait_signal(void) {
	DDRD &= ~_BV(ONEWIRE_DATA_P);
	PORTD &= ~_BV(ONEWIRE_DATA_P);
}

void onewire_send_signal(void) {
	DDRD |= _BV(ONEWIRE_DATA_P);
	PORTD |= _BV(ONEWIRE_DATA_P);
	_delay_ms(1);
	PORTD &= ~_BV(ONEWIRE_DATA_P);
}

uint8_t onewire_receive_data(void) {
	uint8_t d = 0;
	for ( int i = 0; i < 8; i++ ) {
		_delay_ms(1);
		if ( onewire_signal_received() ) {
			d |= ( 1 << i );
		}
	}
	return d;
}

int twpc_data_received() {
	if ( twpc_signal_received ) {
		twpc_signal_received = 0;
		return 1;
	}
	return 0;
}

void twpc_cycle(int send_data) {
	// power cycle
	while ( PIND & _BV(TWPC_DATA_P) );
	while ( !( PIND & _BV(TWPC_DATA_P) ) );
	_twpc_signal_received = 0;
	// data cycle
	if ( send_data ) {
		DDRD |= _BV(TWPC_DATA_P);
		PORTD |= _BV(TWPC_DATA_P);
	}
	TWPC_DELAY();
	DDRD &= ~_BV(TWPC_DATA_P);
	PORTD &= ~_BV(TWPC_DATA_P);
	if ( !send_data && _twpc_signal_received ) {
		twpc_signal_received = 1;
	}
}

int main(void) {
	cli();
	DDRD |= _BV(0);
	DDRB |= _BV(LED_P);
	PORTB &= ~_BV(LED_P);
	PORTD &= ~_BV(TWPC_DATA_P);
	DDRD &= ~_BV(TWPC_DATA_P);
	EICRA = _BV(ISC01) | _BV(ISC11) | _BV(ISC10);
	EIMSK = _BV(INT0) | _BV(INT1);
	sei();
	while ( 1 ) {
		twpc_cycle(0);
		if ( twpc_data_received() ) {
			twpc_cycle(1);
			blink();
		}
		/*
		onewire_send_signal();
		onewire_wait_signal();
		_delay_ms(1);
		if ( onewire_signal_received() ) {
			uint8_t id = onewire_receive_data();
			if ( id == 0x32 ) {
				blink();
			}
			_delay_ms(1000);
		}
		*/
	}
	return 1;
}

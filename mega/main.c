#include <avr/io.h>
#include <avr/interrupt.h>
#include <util/delay.h>

/*
1-wire communication
Slave code

Receive Signal -> send signal + 8bit id

2 wire power communication (TWPC)
Master code
v0.5

Generate power signal + manage slave devices

( http://www.ermicro.com/blog/?p=423 can be useful )
*/

#define LED_P 7
#define ONEWIRE_DATA_P 0
#define TWPC_POWER_P 6
#define TWPC_DATA_P 4

#define TWPC_DELAY() _delay_us(200);

volatile int onewire_signal_caught = 0;
volatile int _twpc_signal_received = 0;
int twpc_signal_received = 0;

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

void onewire_send_data(uint8_t d) {
	for ( int i = 0; i < 8; i++ ) {
		if ( d & _BV(i) ) {
			onewire_send_signal();
		} else {
			_delay_ms(1);
		}
	}
}

ISR(INT0_vect) {
	onewire_signal_caught = 1;
}

ISR(INT4_vect) {
	_twpc_signal_received = 1;
}

void twpc_cycle(int send_data) {
	// power cycle
	PORTB |= _BV(TWPC_POWER_P);
	TWPC_DELAY();
	PORTB &= ~_BV(TWPC_POWER_P);
	_twpc_signal_received = 0;
	// data cycle
	if ( send_data ) {
		PORTB |= _BV(TWPC_POWER_P);
	}
	TWPC_DELAY();
	PORTB &= ~_BV(TWPC_POWER_P);
	if ( !send_data && _twpc_signal_received ) {
		twpc_signal_received = 1;
	}
}

int twpc_data_received() {
	if ( twpc_signal_received ) {
		twpc_signal_received = 0;
		return 1;
	}
	return 0;
}

int main(void) {
	cli();
	DDRB |= _BV(LED_P) | _BV(TWPC_POWER_P);
	PORTB &= ~_BV(LED_P) & ~_BV(TWPC_POWER_P);
	DDRE &= ~_BV(TWPC_DATA_P);
	PORTE &= ~_BV(TWPC_DATA_P);
	EICRA = _BV(ISC01);
	EICRB = _BV(ISC40) | _BV(ISC41);
	EIMSK = _BV(INT0) | _BV(INT4);
	sei();
	uint32_t i = 0;
	while ( 1 ) {
		if ( ++i > 5000 ) {
			i = 0;
			twpc_cycle(1);
			twpc_cycle(0);
			if ( twpc_data_received() ) {
				blink();
			}
		}
		twpc_cycle(0);
		/*
		onewire_wait_signal();
		if ( onewire_signal_received() ) {
			// response
			onewire_send_signal();
			onewire_send_data(0x32);
			// clear signal flag
			onewire_signal_received();
			blink();
		}
		*/
	}
	return 1;
}

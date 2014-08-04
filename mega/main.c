#include <avr/io.h>
#include <avr/interrupt.h>
#include <util/delay.h>

/*
1-wire communication
Master code

Receive Signal -> send signal + 8bit id

2 wire power communication (TWPC)
Master code
v0.6

Generate power signal + manage slave devices

( http://www.ermicro.com/blog/?p=423 can be useful )
*/

#define LED_P 7

#define ONEWIRE_P 0
#define ONEWIRE_DDR DDRD
#define ONEWIRE_PORT PORTD

#define TWPC_POWER_P 6
#define TWPC_POWER_DDR DDRB
#define TWPC_POWER_PORT PORTB

#define TWPC_DATA_P 4
#define TWPC_DATA_DDR DDRE
#define TWPC_DATA_PORT PORTE

#define TWPC_DELAY() _delay_us(200);

volatile static int onewire_signal_caught = 0;
volatile static int twpc_signal_received = 0;

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
	ONEWIRE_DDR &= ~_BV(ONEWIRE_P);
	ONEWIRE_PORT &= ~_BV(ONEWIRE_P);
}

void onewire_send_signal(void) {
	ONEWIRE_DDR |= _BV(ONEWIRE_P);
	ONEWIRE_PORT |= _BV(ONEWIRE_P);
	_delay_ms(1);
	ONEWIRE_PORT &= ~_BV(ONEWIRE_P);
}

uint8_t onewire_receive_data(void) {
	uint8_t d = 0;
	for ( int i = 0; i < 8; i++ ) {
		_delay_ms(1);
		if ( onewire_signal_received() ) {
			d |= _BV(i);
		}
	}
	return d;
}

ISR(INT0_vect) {
	onewire_signal_caught = 1;
}

ISR(INT4_vect) {
	twpc_signal_received = 1;
}

// Master cycle
int twpc_cycle(int send_data) {
	// power cycle
	TWPC_POWER_PORT |= _BV(TWPC_POWER_P);
	TWPC_DELAY();
	twpc_signal_received = 0;
	TWPC_POWER_PORT &= ~_BV(TWPC_POWER_P);
	_delay_us(10);
	// data cycle
	if ( send_data ) {
		TWPC_POWER_PORT |= _BV(TWPC_POWER_P);
	}
	TWPC_DELAY();
	TWPC_POWER_PORT &= ~_BV(TWPC_POWER_P);
	if ( !send_data && twpc_signal_received ) {
		return 1;
	}
	return 0;
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
	DDRB |= _BV(LED_P);
	PORTB &= ~_BV(LED_P);
	twpc_init();
	EICRA = _BV(ISC01);
	EICRB = _BV(ISC40) | _BV(ISC41);
	EIMSK = _BV(INT0) | _BV(INT4);
	sei();
	uint32_t i = 0;
	while ( 1 ) {
		if ( ++i > 3000 ) {
			i = 0;
			twpc_send_data(0x32);
			if ( twpc_cycle(0) && twpc_receive_data() == 0x15 ) {
				blink();
			}
		}
		twpc_cycle(0);
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

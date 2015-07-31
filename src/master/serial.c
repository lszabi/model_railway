#include <avr/io.h>
#include <avr/interrupt.h>
#include "serial.h"

volatile static ring_buffer_t tx;
volatile static ring_buffer_t rx;

void buf_store(char c, volatile ring_buffer_t *b) {
	int i = ( b->end + 1 ) % SERIAL_BUF_SIZE;
	while ( b->start == i );
	b->buffer[b->end] = c;
	b->end = i;
}

char buf_get(volatile ring_buffer_t *b) {
	if ( b->start != b->end ) {
		char c = b->buffer[b->start];
		b->start = ( b->start + 1 ) % SERIAL_BUF_SIZE;
		return c;
	}
	return 0;
}

int buf_available(volatile ring_buffer_t *b) {
	return ( SERIAL_BUF_SIZE + b->end - b->start ) % SERIAL_BUF_SIZE;
}

ISR(USART_RX_vect) {
	buf_store(CAT(UDR, SERIAL_PORT,), &rx);
}

ISR(USART_UDRE_vect) {
	if ( buf_available(&tx) ) {
		CAT(UDR, SERIAL_PORT,) = buf_get(&tx);
	} else {
		CAT(UCSR, SERIAL_PORT, B) &= ~_BV(UDRIE0);
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
	CAT(UBRR, SERIAL_PORT, H) = b >> 8;
	CAT(UBRR, SERIAL_PORT, L) = b;
	CAT(UCSR, SERIAL_PORT, A) |= ( 1 << CAT(U2X, SERIAL_PORT,) );
	CAT(UCSR, SERIAL_PORT, B) |= ( 1 << CAT(RXEN, SERIAL_PORT,) ) | ( 1 << CAT(TXEN, SERIAL_PORT,) ) | ( 1 << CAT(RXCIE, SERIAL_PORT,) );
	CAT(UCSR, SERIAL_PORT, C) |= ( 1 << CAT(UCSZ, SERIAL_PORT, 0) ) | ( 1 << CAT(UCSZ, SERIAL_PORT, 1) );
	sei();
}

void serial_end(void) {
	serial_flush();
	CAT(UCSR, SERIAL_PORT, B) = 0;
}
	
void serial_put(char c) {
	buf_store(c, &tx);
	CAT(UCSR, SERIAL_PORT, B) |= _BV(CAT(UDRIE, SERIAL_PORT,));
}

void serial_puts(char *c) {
	while ( *c ) {
		buf_store(*c++, &tx);
	}
	CAT(UCSR, SERIAL_PORT, B) |= _BV(CAT(UDRIE, SERIAL_PORT,));
}

void serial_putn(char *c, int l) {
	while ( l-- ) {
		buf_store(*c++, &tx);
	}
	CAT(UCSR, SERIAL_PORT, B) |= _BV(CAT(UDRIE, SERIAL_PORT,));
}

int serial_available(void) {
	return buf_available(&rx);
}

char serial_get(void) {
	return buf_get(&rx);
}

int serial_gets(char *msg, int l) {
	int i = 0;
	while ( serial_available() && i < l) {
		msg[i++] = serial_get();
	}
	return i;
}

char serial_wait(void) {
	while ( !serial_available() );
	return serial_get();
}

char serial_peek(void) {
	if ( rx.end != rx.start ) {
		return rx.buffer[rx.start];
	}
	return 0;
}

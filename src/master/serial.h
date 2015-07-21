#ifndef SERIAL_H
#define SERIAL_H

#include <stdint.h>

#define SERIAL_PORT 0

#define SERIAL_BUF_SIZE 256

#define CAT_A(a, b, c) a ## b ## c
#define CAT(a, b, c) CAT_A(a, b, c)

typedef struct {
	char buffer[SERIAL_BUF_SIZE];
	int start;
	int end;
} ring_buffer_t;

void buf_store(char, volatile ring_buffer_t *);
char buf_get(volatile ring_buffer_t *);
int buf_available(volatile ring_buffer_t *);

void serial_flush(void);
void serial_flush_rx(void);

void serial_init(uint16_t);
void serial_end(void);

void serial_put(char);
void serial_puts(char *);

int serial_available(void);

char serial_get(void);
char serial_wait(void);
char serial_peek(void);

#endif
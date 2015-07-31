#ifndef UART_H
#define UART_H

void uart_setup();
void uart_close();

void uart_putchar(char);
int uart_getchar();

int uart_rx(char *, int);
void uart_tx(char *, int);

#endif

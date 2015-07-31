#include <stdio.h>
#include <unistd.h>
#include <fcntl.h>
#include <termios.h>
#include <sys/ioctl.h>
#include <string.h>
#include "uart.h"

/*
RaspberryPi UART software for model railway
connects to Arduino MEGA
created by LSzabi

thanks to wiringPi library for their code
*/

static int uart_stream = -1;

void uart_setup() {
	uart_stream = open("/dev/ttyAMA0", O_RDWR | O_NOCTTY | O_NDELAY | O_NONBLOCK);
	if ( uart_stream == -1 ) {
		printf("Error opening /dev/ttyAMA0\n");
	}
	fcntl(uart_stream, F_SETFL, O_RDWR);
	struct termios options;
	tcgetattr(uart_stream, &options);
	cfmakeraw(&options);
	cfsetispeed(&options, B57600);
	cfsetospeed(&options, B57600);
	options.c_cflag |= CLOCAL | CREAD;
	options.c_cflag &= ~( PARENB | CSTOPB | CSIZE );
	options.c_cflag |= CS8;
	options.c_lflag &= ~( ICANON | ECHO | ECHOE | ISIG );
	options.c_oflag &= ~OPOST;
	options.c_cc[VMIN] = 0;
	options.c_cc[VTIME] = 1; // wait time: n * 100ms
	tcsetattr(uart_stream, TCSANOW | TCSAFLUSH, &options);
	int status;
	ioctl(uart_stream, TIOCMGET, &status);
	status |= TIOCM_DTR | TIOCM_RTS;
	ioctl(uart_stream, TIOCMSET, &status);
	usleep(10000);
	tcflush(uart_stream, TCIFLUSH);
}

void uart_close() {
	close(uart_stream);
}

void uart_putchar(char c) {
	int count = write(uart_stream, &c, 1);
	if ( count != 1 ) {
		printf("UART TX error (in putchar)\n");
	}
}

int uart_getchar() {
	char c;
	if ( read(uart_stream, &c, 1) != 1 ) {
		return -1;
	}
	return ( (int)c ) & 0xFF;
}

int uart_rx(char *s, int n) {
	return read(uart_stream, s, n);
	/*
	int i;
	for ( i = 0; i < n; i++ ) {
		int c = uart_getchar();
		if ( c == -1 ) {
			break;
		}
		s[i] = c;
	}
	s[i] = '\0';
	return i;
	*/
}

void uart_tx(char *s, int n) {
	int count = write(uart_stream, s, n);
	if ( count != n ) {
		printf("UART TX error\n");
	}
	/*
	int i;
	for ( i = 0; i < n; i++ ) {
		uart_putchar(s[i]);
	}
	*/
}

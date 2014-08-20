#include <stdio.h>
#include <unistd.h>
#include <fcntl.h>
#include <termios.h>
#include <sys/ioctl.h>
#include <string.h>

/*
RaspberryPi UART software for model railway
connects to Arduino MEGA
created by LSzabi

thanks to wiringPi library for their code
*/

int uart_stream = -1;

// setting up uart
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
	options.c_cc[VTIME] = 5; // wait time: seconds * 10
	tcsetattr(uart_stream, TCSANOW | TCSAFLUSH, &options);
	int status;
	ioctl(uart_stream, TIOCMGET, &status);
	status |= TIOCM_DTR | TIOCM_RTS;
	ioctl(uart_stream, TIOCMSET, &status);
	usleep(10000);
	tcflush(uart_stream, TCIFLUSH);
}

// transmit one char
void uart_putchar(char c) {
	int count = write(uart_stream, &c, 1); // using write() and read() with strings of more chars won't work somehow
	if ( count != 1 ) {
		printf("UART TX error\n");
	}
}

// receive one char
int uart_getchar() {
	char c;
	if ( read(uart_stream, &c, 1) != 1 ) {
		return -1;
	}
	return ( (int)c ) & 0xFF;
}

// receive string
int uart_rx(char *s, int n) {
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
}

// transmit string
void uart_tx(char *s, int n) {
	int i;
	for ( i = 0; i < n; i++ ) {
		uart_putchar(s[i]);
	}
}

// end connection
void uart_close() {
	close(uart_stream);
}

int main(int argc, char **argv) {
	if ( argc > 1 ) {
		uart_setup();
		uart_tx(argv[1], strlen(argv[1]));
		char s[128];
		uart_rx(s, 128);
		printf("%s", s); // this will be displayed by the PHP script
		uart_close();
	} else {
		printf("Nothing to send");
	}
	return 0;
}

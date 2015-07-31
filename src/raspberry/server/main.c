//#include "socket.h"
#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <stdint.h>
#include <signal.h>
#include "socket.h"
#include "websocket.h"
#include "uart.h"
#include "control.h"
#include "../../twpc_def.h"

static int sockets[512];
static int running = 1;

void signal_close(int n) {
	running = 0;
}

void socket_list_close(int *sock) {
	printf("Socket closed\n");
	socket_close(*sock);
	*sock = -1;
}

int main (int argc, char * argv[]) {
	int port = 9090;
	if ( argc > 1 ) {
		port = atoi(argv[1]);
	}
	uart_setup();
	signal(SIGINT, signal_close);
	signal(SIGTERM, signal_close);
	int sock_listen = socket_create();
	if ( socket_listen(sock_listen, port) == -1 ) {
		printf("Bind failed\n");
		return 1;
	}
	for ( int i = 0; i < 512; i++ ) {
		sockets[i] = -1;
	}
	char msg[512];
	twpc_packet_t serial_packet;
	printf("Server started.\n");
	while ( running ) {
		// Receive data from mcu
		memset(msg, 0, sizeof(char) * 512);
		uart_rx(msg, 512);
		if ( strlen(msg) > 0 ) {
			printf("Received from serial: %s\n", msg);
		}
		// Receive data from connected clients
		for ( int i = 0; i < 512; i++ ) {
			if ( sockets[i] != -1 ) {
				// There is data to receive
				int len = websocket_recv(sockets[i], msg);
				if ( len < 0 ) {
					// Failed
					socket_list_close(&(sockets[i]));
				} else if ( len > 0 ) {
					// Handle and send data
					printf("Received: '%s'\n", msg);
					if ( control_handle(msg, &serial_packet) == 0 ) {
						serial_packet.checksum = TWPC_CHECKSUM(serial_packet);
						uart_tx((char *)&serial_packet, sizeof(twpc_packet_t));
					}
				}
			}
		}
		// Accept connection of new clients
		int sock_accept = socket_accept(sock_listen);
		if ( sock_accept > 0 ) {
			printf("Connection accepted\n");
			if ( websocket_accept(sock_accept) ) {
				int found = 0;
				for ( int i = 0; i < 512; i++ ) {
					if ( sockets[i] == -1 ) {
						sockets[i] = sock_accept;
						found = 1;
						break;
					}
				}
				if ( !found ) {
					printf("No room for more connections");
					socket_close(sock_accept);
				}
			} else {
				printf("Accept failed\n");
				socket_close(sock_accept);
			}
		}
	}
	uart_close();
	for ( int i = 0; i < 512; i++ ) {
		if ( sockets[i] != -1 ) {
			socket_list_close(&sockets[i]);
		}
	}
	socket_close(sock_listen);
	WSA_CLEAN();
	return 0;
}

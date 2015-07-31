#include "socket.h"
#include <string.h>

/*
 * Socket lib by L Szabi 2014
 * v0.9
 */

#ifdef WIN32

static void socket_nonblock(int socket_id) {
	u_long a = 1;
	ioctlsocket(socket_id, FIONBIO, &a);
}

#else

#include <fcntl.h>
static void socket_nonblock(int socket_id) {
	fcntl(socket_id, F_SETFL, O_NONBLOCK);
}

#endif

int socket_create() {
#ifdef WIN32
	WSADATA wsa;
	if ( WSAStartup(MAKEWORD(2, 2), &wsa) ) {
		return -1;
	}
#endif
	int socket_id = socket(AF_INET, SOCK_STREAM, 0);
	if ( socket_id < 0 ) {
		WSA_CLEAN();
		return -1;
	}
	return socket_id;
}

int socket_listen(int socket_id, int port) {
	struct sockaddr_in address;
	memset(&address, 0, sizeof(struct sockaddr_in));
	address.sin_family = AF_INET;
	address.sin_addr.s_addr = INADDR_ANY;
	address.sin_port = htons(port);
	int yes = 1;
	if ( setsockopt(socket_id, SOL_SOCKET, SO_REUSEADDR, &yes, sizeof(yes)) < 0 ) {
		return -1;
	}
	if ( bind(socket_id, (struct sockaddr *)&address, sizeof(address)) < 0 ) {
		return -1;
	}
	listen(socket_id, 5);
	socket_nonblock(socket_id);
	return 0;
}

int socket_connect(int socket_id, char *ip, int port) {
	struct sockaddr_in address;
	memset(&address, 0, sizeof(struct sockaddr_in));
	address.sin_family = AF_INET;
	address.sin_addr.s_addr = inet_addr(ip);
	address.sin_port = htons(port);
	if ( connect(socket_id, (const struct sockaddr *)&address, sizeof(address)) < 0 ) {
		return -1;
	}
	socket_nonblock(socket_id);
	return 0;
}

void socket_close(int socket_id) {
	shutdown(socket_id, SHUT_RDWR);
	close(socket_id);
}

int socket_accept(int socket_id) {
	int result = accept(socket_id, NULL, NULL);
	if ( result < 0 && SOCKET_ERROR_FUNCTION() != SOCKET_AGAIN ) {
		return -1;
	} else if ( result < 0 ) {
		return 0;
	}
	socket_nonblock(result);
	return result;
}

int socket_recv(int socket_id, char *buffer, int size) {
	memset(buffer, 0, sizeof(char) * size);
	int result = recv(socket_id, buffer, size, 0);
	if ( result == 0 || ( result < 0 && SOCKET_ERROR_FUNCTION() != SOCKET_AGAIN ) ) {
		return -1;
	} else if ( result < 0 ) {
		return 0;
	}
	return result;
}

int socket_send(int socket_id, char *buffer, int length) {
	int result = send(socket_id, buffer, length, 0);
	if ( result < 0 || result != length ) {
		return -1;
	}
	return 0;
}

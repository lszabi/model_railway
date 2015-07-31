#ifndef SOCKET_H
#define SOCKET_H

/*
 * Non-blocking socket library by L Szabi 2014
 * for Windows and Linux
 * v0.9
 */

#include <unistd.h>

#ifdef WIN32
	#include <winsock2.h> // -lws2_32
	#include <windows.h>
	#define WSA_CLEAN() WSACleanup()
	#define SOCKET_ERROR_FUNCTION() WSAGetLastError()
	#define SOCKET_AGAIN WSAEWOULDBLOCK
	typedef int socklen_t;
	#ifndef SHUT_RDWR
		#define SHUT_RDWR SD_BOTH
	#endif
#else
	#include <sys/socket.h>
	#include <netinet/in.h>
	#include <arpa/inet.h>
	#include <netdb.h>
	#include <errno.h>
	#define WSA_CLEAN()
	#define SOCKET_ERROR_FUNCTION() errno
	#ifndef EWOULDBLOCK
		#ifndef EAGAIN
			#error "Socket error code is missing"
		#else
			#define SOCKET_AGAIN EAGAIN
		#endif
	#else
		#define SOCKET_AGAIN EWOULDBLOCK
	#endif
#endif // WIN32

int socket_create();
int socket_listen(int, int);
int socket_connect(int, char *, int);
void socket_close(int);
int socket_accept(int);
int socket_recv(int, char *, int);
int socket_send(int, char *, int);

#endif
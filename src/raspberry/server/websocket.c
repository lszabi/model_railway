#include <string.h>
#include <stdlib.h>
#include <stdio.h>
#include "socket.h"
#include "sha1.h"
#include "websocket.h"

static const char guid[] = "258EAFA5-E914-47DA-95CA-C5AB0DC85B11";

static int mod_table[] = {0, 2, 1};

static char encoding_table[] = {
	'A', 'B', 'C', 'D', 'E', 'F', 'G', 'H', 'I', 'J', 'K', 'L', 'M', 'N', 'O', 'P', 'Q', 'R', 'S', 'T', 'U', 'V', 'W', 'X', 'Y', 'Z',
	'a', 'b', 'c', 'd', 'e', 'f', 'g', 'h', 'i', 'j', 'k', 'l', 'm', 'n', 'o', 'p', 'q', 'r', 's', 't', 'u', 'v', 'w', 'x', 'y', 'z',
	'0', '1', '2', '3', '4', '5', '6', '7', '8', '9', '+', '/'
};

char *base64_encode(const unsigned char *data, size_t input_length) {
	int output_length = 4 * ( ( input_length + 2 ) / 3 );
	char *encoded_data = (char *)malloc(sizeof(char) * ( output_length + 1 ));
	if ( encoded_data == NULL ) {
		return NULL;
	}
	for ( int i = 0, j = 0; i < input_length; ) {
		uint32_t octet_a = i < input_length ? (unsigned char)data[i++] : 0;
		uint32_t octet_b = i < input_length ? (unsigned char)data[i++] : 0;
		uint32_t octet_c = i < input_length ? (unsigned char)data[i++] : 0;
		uint32_t triple = (octet_a << 0x10) + (octet_b << 0x08) + octet_c;
		encoded_data[j++] = encoding_table[(triple >> 3 * 6) & 0x3F];
		encoded_data[j++] = encoding_table[(triple >> 2 * 6) & 0x3F];
		encoded_data[j++] = encoding_table[(triple >> 1 * 6) & 0x3F];
		encoded_data[j++] = encoding_table[(triple >> 0 * 6) & 0x3F];
	}
	for ( int i = 0; i < mod_table[input_length % 3]; i++ ) {
		encoded_data[output_length - 1 - i] = '=';
	}
	encoded_data[output_length] = '\0';
	return encoded_data;
}

static uint32_t read_octet(unsigned char *buffer, int start) {
	return buffer[start] << 24 | buffer[start + 1] << 16 | buffer[start + 2] << 8 | buffer[start + 3];
}

static uint32_t read_bits(unsigned char *buffer, int frame, int start_bit, int bits) {
	uint32_t frame_data = read_octet(buffer, 4 * frame);
	return ( frame_data >> start_bit ) & ( ( 1 << bits ) - 1 );
}

static char *websocket_key(const char *input) {
	char *output = (char *)malloc(sizeof(char) * 64);
	memset(output, 0, sizeof(char) * 64);
	const char search[] = "Sec-WebSocket-Key: ";
	char *start = strstr(input, search);
	if ( start == NULL ) {
		return NULL;
	}
	start += strlen(search);
	char *end = strstr((const char *)start, "\r");
	if ( end == NULL ) {
		end = strstr((const char *)start, "\n");
	}
	strncpy(output, start, end - start);
	strcat(output, guid);
	char sha1[20];
	SHA1String(output, sha1);
	char *base64 = base64_encode((const unsigned char *)sha1, 20);
	free(output);
	return base64;
}

int websocket_accept(int sock) {
	char msg[512];
	int len = 0;
	while ( len == 0 ) {
		len = socket_recv(sock, msg, 512);
	}
	if ( len < 0 ) {
		return 0;
	}
	msg[len] = '\0';
	char *key = websocket_key(msg);
	sprintf(msg, "HTTP/1.1 101 Switching Protocols\r\n"
		"Upgrade: websocket\r\n"
		"Connection: Upgrade\r\n"
		"Sec-WebSocket-Accept: %s\r\n\r\n", key);
	free(key);
	if ( socket_send(sock, msg, strlen(msg)) < 0 ) {
		return 0;
	}
	return 1;
}

int websocket_recv(int sock, char *msg) {
	char buffer[512];
	int len = socket_recv(sock, buffer, 512);
	if ( len < 0 ) {
		return -1;
	} else if ( len > 0 ) {
		int opcode = read_bits((unsigned char *)buffer, 0, 24, 4);
		if ( opcode == 0x01 ) {
			int pl_len = read_bits((unsigned char *)buffer, 0, 16, 7);
			uint32_t mask_key = read_octet((unsigned char *)buffer, 2);
			int i;
			for ( i = 0; i < pl_len; i++ ) {
				msg[i] = ((unsigned char *)buffer)[6 + i] ^ ((unsigned char *)buffer)[2 + i % 4];
			}
			msg[i] = '\0';
			return strlen(msg);
		} else if ( opcode == 0x08 ) {
			return -1;
		}
	}
	return 0;
}

int websocket_send(int sock, char *msg) {
	char response[512];
	response[0] = 0x81;
	response[1] = strlen(msg);
	sprintf(&(response[2]), "%s", msg);
	return socket_send(sock, response, 2 + strlen(msg));
}

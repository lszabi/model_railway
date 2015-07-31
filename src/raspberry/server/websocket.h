#ifndef WEBSOCKET_H
#define WEBSOCKET_H

#include <stdlib.h>

char *base64_encode(const unsigned char *, size_t);

int websocket_accept(int);
int websocket_recv(int, char *);
int websocket_send(int, char *);

#endif

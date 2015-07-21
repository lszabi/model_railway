#ifndef TWPC_DEF_H
#define TWPC_DEF_H

#include <stdint.h>

#define TWPC_DATA_BITS ( sizeof(twpc_packet_t) * 8 )
#define TWPC_BUFFER_SIZE 32

typedef union {
	uint32_t data_raw;
	struct {
		uint8_t uid;
		uint8_t cmd;
		uint16_t arg;
	} __attribute__((packed));
} __attribute__((packed)) twpc_packet_t;

#define TWPC_CMD_LIGHT_ON	0x01
#define TWPC_CMD_LIGHT_OFF	0x02
#define TWPC_CMD_MOTOR		0x03

#endif
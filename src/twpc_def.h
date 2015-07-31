#ifndef TWPC_DEF_H
#define TWPC_DEF_H

#include <stdint.h>

#define TWPC_DATA_BITS ( sizeof(twpc_packet_t) * 8 )
#define TWPC_BUFFER_SIZE 32

#define TWPC_CHECKSUM(a) ( ( ( (a).uid & 0x03 ) << 4 ) | ( ( (a).cmd & 0x03 ) << 2 ) | ( (a).arg & 0x03 ) )

typedef union {
	uint32_t data_raw;
	struct {
		uint8_t uid;
		uint8_t cmd;
		uint8_t arg;
		uint8_t checksum;
	} __attribute__((packed));
} __attribute__((packed)) twpc_packet_t;

#define TWPC_CMD_LIGHT_ON	0x01
#define TWPC_CMD_LIGHT_OFF	0x02
#define TWPC_CMD_MOTOR_A	0x03
#define TWPC_CMD_MOTOR_B	0x04
#define TWPC_CMD_STATUS		0x05
#define TWPC_CMD_NAME		0x06

#endif
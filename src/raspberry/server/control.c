#include "control.h"

int hex_to_int(char *hex, int l) {
	int n = 0;
	for ( int i = 0; hex[i] && i < l; i++ ) {
		n <<= 4;
		if ( hex[i] >= '0' && hex[i] <= '9' ) {
			n += hex[i] - '0';
		} else if ( hex[i] >= 'A' && hex[i] <= 'F' ) {
			n += hex[i] - 'A' + 0x0A;
		} else if ( hex[i] >= 'a' && hex[i] <= 'f' ) {
			n += hex[i] - 'a' + 0x0A;
		}
	}
	return n;
}

int control_handle(char *msg, twpc_packet_t *packet) {
	char c = msg[0];
	if ( c == 'l' ) {
		int uid = hex_to_int(&msg[1], 2);
		int state = hex_to_int(&msg[3], 2);
		packet->uid = uid;
		packet->arg = 0;
		packet->cmd = state ? TWPC_CMD_LIGHT_ON : TWPC_CMD_LIGHT_OFF;
	} else if ( c == 'm' ) {
		int uid = hex_to_int(&msg[1], 2);
		int dir = hex_to_int(&msg[3], 2);
		int speed = hex_to_int(&msg[5], 2);
		packet->uid = uid;
		packet->arg = speed;
		packet->cmd = dir ? TWPC_CMD_MOTOR_B : TWPC_CMD_MOTOR_A;
	} else if ( c == 's' ) {
		int uid = hex_to_int(&msg[1], 2);
		int s = hex_to_int(&msg[3], 2);
		packet->uid = uid;
		packet->arg = s;
		packet->cmd = TWPC_CMD_STATUS;
	} else {
		return -1;
	}
	return 0;
}

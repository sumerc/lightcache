
#ifndef PROTOCOL_H
#define PROTOCOL_H

typedef enum {
	CMD_GET = 0x00,
	CMD_SET = 0x01,
	CMD_CHG_SETTING = 0x02,
	CMD_GET_SETTING = 0x03,
} protocol_commands;

#endif
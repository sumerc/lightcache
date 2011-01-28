
#ifndef PROTOCOL_H
#define PROTOCOL_H

#define PROTOCOL_MAX_EXTRA_SIZE 250 // in bytes --
#define PROTOCOL_MAX_KEY_SIZE 250 // in bytes --
#define PROTOCOL_MAX_DATA_SIZE 1024 + PROTOCOL_MAX_KEY_SIZE // in bytes -- same as memcached

#define ITEM_XINCREF(x) (x.ref_count = 1);
#define ITEM_XDECREF(x) (x.ref_count = 0);

/* an item with reference count */
typedef struct item item;
struct item {
	char *data;
	unsigned int ref_count;
};

typedef union {
	struct {
		uint8_t opcode;
		uint8_t key_length;		
		uint32_t data_length;	
		uint32_t extra_length;	
	};
	uint8_t bytes[12];
}req_header;

typedef union {
	struct {
		uint8_t opcode;
		uint32_t data_length;
	};
	uint8_t bytes[8];
}resp_header;

typedef struct request request;
struct request {
	req_header req_header;
	item rdata;
	char *rkey;
	char *rextra;
	unsigned int rbytes; /*current read index*/
};

typedef struct response response;
struct response {
	resp_header resp_header;
	item sdata;
	unsigned int sbytes; /*current write index*/
};

typedef enum {
	CMD_GET = 0x00,
	CMD_SET = 0x01,
	CMD_CHG_SETTING = 0x02,
	CMD_GET_SETTING = 0x03,
	CMD_GET_STATS = 0x04,
} protocol_commands;

#endif
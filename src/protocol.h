
#ifndef PROTOCOL_H
#define PROTOCOL_H

#define PROTOCOL_MAX_EXTRA_SIZE 250 // in bytes --
#define PROTOCOL_MAX_KEY_SIZE 250 // in bytes --
#define PROTOCOL_MAX_DATA_SIZE 1024 + PROTOCOL_MAX_KEY_SIZE // in bytes -- same as memcached

typedef union req_header {
    struct  {
        uint8_t opcode;
        uint8_t key_length;
        uint32_t data_length;
        uint32_t extra_length;
    } request;
    uint8_t bytes[12];
} req_header;

typedef union {
    struct {
        uint8_t opcode;
        uint8_t errcode;
        uint32_t data_length;
    } response;
    uint8_t bytes[8];
} resp_header;

typedef struct request {
    req_header req_header;
    char *rdata;
    char *rkey;
    char *rextra;
    unsigned int rbytes; /* current index in to the receive buf */
    time_t received;
    int can_free; /* flag to indicate whether data can be freed. */
} request;

typedef struct response_data {
    char *data;
    unsigned int sbytes;
    int can_free;
    struct response_data *next;
} response_data;

typedef struct response_data_queue {
    response_data *head;
    response_data *tail;
    unsigned int nitems;
} response_data_queue;

typedef struct response {
    resp_header resp_header;
    char *sdata;
    unsigned int sbytes; /*current write index*/

    response_data_queue sdata_vec;
    
    int can_free;
} response;

typedef enum {
    CMD_GET = 0x00,
    CMD_SET = 0x01,
    CMD_CHG_SETTING = 0x02,
    CMD_GET_SETTING = 0x03,
    CMD_GET_STATS = 0x04,
    CMD_DELETE = 0x05,
    CMD_FLUSH_ALL = 0x06,
    CMD_GETQ = 0x07,
    CMD_SETQ = 0x08,
} protocol_commands;

typedef enum {
    READ_HEADER = 0x00,
    READ_KEY = 0x01,
    READ_DATA = 0x02,
    CONN_CLOSED = 0x03,
    CMD_RECEIVED = 0x04,
    SEND_HEADER = 0x05,
    SEND_DATA = 0x06,
    READ_EXTRA = 0x07,
    CMD_SENT = 0x08,
} conn_states;

typedef enum {
    KEY_NOTEXISTS = 0x00,
    INVALID_PARAM = 0x01,
    INVALID_STATE = 0x02,
    INVALID_PARAM_SIZE = 0x03,
    SUCCESS = 0x04,
    INVALID_COMMAND = 0x05,
    OUT_OF_MEMORY = 0x06,
} errors;

typedef struct conn {
    int fd; 						/* socket fd */
    int listening;					/* listening socket? */
    int active;					    /* conn have active events on the I/O interface */
    time_t last_heard; 				/* last time we heard from the client */
    conn_states state; 				/* state of the connection READ_KEY, READ_HEADER.etc...*/
    request *in;					/* request instance */
    response *out;					/* response instance */
    int queue_responses;            /* flag indicates for queueing the responses. */
    int free; 						/* recycle connection structure */
    struct conn *next;						/* next connection in the linked-list of the connections */
} conn;

#endif

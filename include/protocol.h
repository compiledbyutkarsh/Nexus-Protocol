#ifndef PROTOCOL_H
#define PROTOCOL_H

#include <stdint.h>
#include <stddef.h>
#include <sys/types.h>

#define PROTO_MAGIC          0xDEADBEEF
#define PROTO_VERSION        1
#define PROTO_MAX_PAYLOAD    65535
#define PROTO_HEADER_SIZE    20
#define PROTO_KEEPALIVE_MS   5000
#define PROTO_TIMEOUT_MS     15000

typedef enum {
    FRAME_HANDSHAKE     = 0x01,
    FRAME_HANDSHAKE_ACK = 0x02,
    FRAME_DATA          = 0x03,
    FRAME_ACK           = 0x04,
    FRAME_NACK          = 0x05,
    FRAME_KEEPALIVE     = 0x06,
    FRAME_FIN           = 0x07,
    FRAME_FIN_ACK       = 0x08,
} frame_type_t;

typedef enum {
    CONN_CLOSED      = 0,
    CONN_SYN_SENT,
    CONN_SYN_RECV,
    CONN_ESTABLISHED,
    CONN_FIN_WAIT,
    CONN_CLOSE_WAIT,
    CONN_TIME_WAIT,
} conn_state_t;

typedef enum {
    PROTO_OK         =  0,
    PROTO_ERR_MAGIC  = -1,
    PROTO_ERR_CRC    = -2,
    PROTO_ERR_LEN    = -3,
    PROTO_ERR_SEQ    = -4,
    PROTO_ERR_STATE  = -5,
    PROTO_ERR_IO     = -6,
    PROTO_ERR_TIMEOUT= -7,
} proto_err_t;

typedef struct __attribute__((packed)) {
    uint32_t magic;
    uint8_t  version;
    uint8_t  type;
    uint16_t flags;
    uint32_t seq;
    uint32_t payload_len;
    uint32_t checksum;
} frame_header_t;

typedef struct {
    frame_header_t  header;
    uint8_t        *payload;
} frame_t;

typedef struct {
    int           fd;
    conn_state_t  state;
    uint32_t      local_seq;
    uint32_t      remote_seq;
    uint32_t      last_ack;
} connection_t;

uint32_t    crc32_compute(const uint8_t *data, size_t len);

int         frame_encode(const frame_t *frame, uint8_t *buf, size_t buf_len);
int         frame_decode(const uint8_t *buf, size_t buf_len, frame_t *out);
void        frame_free(frame_t *frame);

int         transport_connect(const char *host, uint16_t port);
int         transport_listen(uint16_t port);
int         transport_accept(int server_fd);
ssize_t     transport_send(int fd, const uint8_t *buf, size_t len);
ssize_t     transport_recv(int fd, uint8_t *buf, size_t len);
void        transport_close(int fd);

int         conn_init(connection_t *conn, int fd);
int         conn_handshake_client(connection_t *conn);
int         conn_handshake_server(connection_t *conn);
int         conn_send(connection_t *conn, const uint8_t *data, size_t len);
int         conn_recv(connection_t *conn, uint8_t *buf, size_t buf_len);
int         conn_close(connection_t *conn);
const char *conn_state_str(conn_state_t state);

#endif

int reliable_send(connection_t *conn, const uint8_t *data, size_t len);
int reliable_recv(connection_t *conn, uint8_t *buf, size_t buf_len);

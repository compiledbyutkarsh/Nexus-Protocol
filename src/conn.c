#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <time.h>
#include <arpa/inet.h>
#include "protocol.h"

static uint32_t generate_isn(void)
{
    struct timespec ts;
    clock_gettime(CLOCK_MONOTONIC, &ts);
    return (uint32_t)(ts.tv_nsec ^ (ts.tv_sec << 16));
}

static int send_ctrl_frame(int fd, frame_type_t type, uint32_t seq)
{
    uint8_t buf[PROTO_HEADER_SIZE];
    frame_t frame;
    memset(&frame, 0, sizeof(frame));
    frame.header.type        = type;
    frame.header.seq         = seq;
    frame.header.payload_len = 0;
    frame.payload            = NULL;

    int n = frame_encode(&frame, buf, sizeof(buf));
    if (n < 0)
        return n;

    ssize_t sent = transport_send(fd, buf, (size_t)n);
    if (sent < 0)
        return PROTO_ERR_IO;

    return PROTO_OK;
}

static int recv_ctrl_frame(int fd, frame_t *out)
{
    uint8_t buf[PROTO_HEADER_SIZE];

    ssize_t n = transport_recv(fd, buf, PROTO_HEADER_SIZE);
    if (n < 0)
        return PROTO_ERR_IO;

    uint32_t magic_be;
    memcpy(&magic_be, buf, 4);
    if (ntohl(magic_be) != PROTO_MAGIC)
        return PROTO_ERR_MAGIC;

    uint32_t plen_be;
    memcpy(&plen_be, buf + offsetof(frame_header_t, payload_len), 4);
    uint32_t plen = ntohl(plen_be);

    if (plen > PROTO_MAX_PAYLOAD)
        return PROTO_ERR_LEN;

    uint8_t *full = malloc(PROTO_HEADER_SIZE + plen);
    if (!full)
        return PROTO_ERR_IO;

    memcpy(full, buf, PROTO_HEADER_SIZE);

    if (plen > 0) {
        n = transport_recv(fd, full + PROTO_HEADER_SIZE, plen);
        if (n < 0) {
            free(full);
            return PROTO_ERR_IO;
        }
    }

    int ret = frame_decode(full, PROTO_HEADER_SIZE + plen, out);
    free(full);
    return ret;
}

const char *conn_state_str(conn_state_t state)
{
    switch (state) {
        case CONN_CLOSED:      return "CLOSED";
        case CONN_SYN_SENT:    return "SYN_SENT";
        case CONN_SYN_RECV:    return "SYN_RECV";
        case CONN_ESTABLISHED: return "ESTABLISHED";
        case CONN_FIN_WAIT:    return "FIN_WAIT";
        case CONN_CLOSE_WAIT:  return "CLOSE_WAIT";
        case CONN_TIME_WAIT:   return "TIME_WAIT";
        default:               return "UNKNOWN";
    }
}

int conn_init(connection_t *conn, int fd)
{
    if (!conn || fd < 0)
        return PROTO_ERR_IO;

    memset(conn, 0, sizeof(*conn));
    conn->fd         = fd;
    conn->state      = CONN_CLOSED;
    conn->local_seq  = generate_isn();
    conn->remote_seq = 0;
    conn->last_ack   = 0;

    return PROTO_OK;
}

int conn_handshake_client(connection_t *conn)
{
    if (!conn || conn->state != CONN_CLOSED)
        return PROTO_ERR_STATE;

    fprintf(stdout, "[conn] initiating handshake (isn=%u)\n", conn->local_seq);

    int ret = send_ctrl_frame(conn->fd, FRAME_HANDSHAKE, conn->local_seq);
    if (ret != PROTO_OK)
        return ret;

    conn->state = CONN_SYN_SENT;
    fprintf(stdout, "[conn] state -> %s\n", conn_state_str(conn->state));

    frame_t resp;
    memset(&resp, 0, sizeof(resp));
    ret = recv_ctrl_frame(conn->fd, &resp);
    if (ret != PROTO_OK)
        goto fail;

    if (resp.header.type != FRAME_HANDSHAKE_ACK) {
        ret = PROTO_ERR_STATE;
        goto fail;
    }

    conn->remote_seq = resp.header.seq;
    conn->last_ack   = resp.header.seq;
    frame_free(&resp);

    ret = send_ctrl_frame(conn->fd, FRAME_ACK, conn->local_seq);
    if (ret != PROTO_OK)
        goto fail;

    conn->state = CONN_ESTABLISHED;
    fprintf(stdout, "[conn] state -> %s (remote_isn=%u)\n",
            conn_state_str(conn->state), conn->remote_seq);

    return PROTO_OK;

fail:
    conn->state = CONN_CLOSED;
    fprintf(stderr, "[conn] handshake failed: %d\n", ret);
    return ret;
}

int conn_handshake_server(connection_t *conn)
{
    if (!conn || conn->state != CONN_CLOSED)
        return PROTO_ERR_STATE;

    frame_t syn;
    memset(&syn, 0, sizeof(syn));
    int ret = recv_ctrl_frame(conn->fd, &syn);
    if (ret != PROTO_OK)
        return ret;

    if (syn.header.type != FRAME_HANDSHAKE) {
        fprintf(stderr, "[conn] expected HANDSHAKE, got type=%u\n", syn.header.type);
        frame_free(&syn);
        return PROTO_ERR_STATE;
    }

    conn->remote_seq = syn.header.seq;
    conn->state      = CONN_SYN_RECV;
    frame_free(&syn);

    fprintf(stdout, "[conn] state -> %s (client_isn=%u)\n",
            conn_state_str(conn->state), conn->remote_seq);

    ret = send_ctrl_frame(conn->fd, FRAME_HANDSHAKE_ACK, conn->local_seq);
    if (ret != PROTO_OK)
        goto fail;

    frame_t ack;
    memset(&ack, 0, sizeof(ack));
    ret = recv_ctrl_frame(conn->fd, &ack);
    if (ret != PROTO_OK)
        goto fail;

    if (ack.header.type != FRAME_ACK) {
        frame_free(&ack);
        ret = PROTO_ERR_STATE;
        goto fail;
    }

    conn->last_ack = ack.header.seq;
    frame_free(&ack);

    conn->state = CONN_ESTABLISHED;
    fprintf(stdout, "[conn] state -> %s\n", conn_state_str(conn->state));

    return PROTO_OK;

fail:
    conn->state = CONN_CLOSED;
    fprintf(stderr, "[conn] handshake failed: %d\n", ret);
    return ret;
}

int conn_send(connection_t *conn, const uint8_t *data, size_t len)
{
    if (!conn || conn->state != CONN_ESTABLISHED)
        return PROTO_ERR_STATE;
    return reliable_send(conn, data, len);
}

int conn_recv(connection_t *conn, uint8_t *buf, size_t buf_len)
{
    if (!conn || conn->state != CONN_ESTABLISHED)
        return PROTO_ERR_STATE;
    return reliable_recv(conn, buf, buf_len);
}

int conn_close(connection_t *conn)
{
    if (!conn || conn->state == CONN_CLOSED)
        return PROTO_OK;

    fprintf(stdout, "[conn] initiating teardown\n");

    send_ctrl_frame(conn->fd, FRAME_FIN, conn->local_seq);
    conn->state = CONN_FIN_WAIT;
    fprintf(stdout, "[conn] state -> %s\n", conn_state_str(conn->state));

    frame_t fin_ack;
    memset(&fin_ack, 0, sizeof(fin_ack));
    int ret = recv_ctrl_frame(conn->fd, &fin_ack);

    if (ret == PROTO_OK && fin_ack.header.type == FRAME_FIN_ACK) {
        send_ctrl_frame(conn->fd, FRAME_ACK, conn->local_seq);
        conn->state = CONN_TIME_WAIT;
        fprintf(stdout, "[conn] state -> %s\n", conn_state_str(conn->state));
        frame_free(&fin_ack);
    }

    transport_close(conn->fd);
    conn->state = CONN_CLOSED;
    fprintf(stdout, "[conn] state -> %s\n", conn_state_str(conn->state));
    return PROTO_OK;
}

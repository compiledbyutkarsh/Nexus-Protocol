#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include "protocol.h"

#define MAX_RETRIES     5
#define RETRY_DELAY_MS  200

static int send_frame(int fd, frame_t *frame)
{
    uint8_t buf[PROTO_HEADER_SIZE + PROTO_MAX_PAYLOAD];
    int     n = frame_encode(frame, buf, sizeof(buf));
    if (n < 0)
        return n;
    return transport_send(fd, buf, (size_t)n) < 0 ? PROTO_ERR_IO : PROTO_OK;
}

static int recv_frame(int fd, frame_t *out)
{
    uint8_t buf[PROTO_HEADER_SIZE + PROTO_MAX_PAYLOAD];

    ssize_t n = transport_recv(fd, buf, PROTO_HEADER_SIZE);
    if (n < 0)
        return PROTO_ERR_IO;

    uint32_t plen;
    memcpy(&plen, buf + offsetof(frame_header_t, payload_len), 4);
    plen = ntohl(plen);

    if (plen > PROTO_MAX_PAYLOAD)
        return PROTO_ERR_LEN;

    if (plen > 0) {
        n = transport_recv(fd, buf + PROTO_HEADER_SIZE, plen);
        if (n < 0)
            return PROTO_ERR_IO;
    }

    return frame_decode(buf, PROTO_HEADER_SIZE + plen, out);
}

static int send_ack(int fd, uint32_t seq)
{
    frame_t ack;
    memset(&ack, 0, sizeof(ack));
    ack.header.type        = FRAME_ACK;
    ack.header.seq         = seq;
    ack.header.payload_len = 0;
    ack.payload            = NULL;
    return send_frame(fd, &ack);
}

static int send_nack(int fd, uint32_t seq)
{
    frame_t nack;
    memset(&nack, 0, sizeof(nack));
    nack.header.type        = FRAME_NACK;
    nack.header.seq         = seq;
    nack.header.payload_len = 0;
    nack.payload            = NULL;
    return send_frame(fd, &nack);
}

int reliable_send(connection_t *conn, const uint8_t *data, size_t len)
{
    if (!conn || !data || len == 0)
        return PROTO_ERR_IO;

    frame_t frame;
    memset(&frame, 0, sizeof(frame));
    frame.header.type        = FRAME_DATA;
    frame.header.seq         = conn->local_seq;
    frame.header.payload_len = (uint32_t)len;
    frame.payload            = (uint8_t *)data;

    for (int attempt = 0; attempt < MAX_RETRIES; attempt++) {
        int ret = send_frame(conn->fd, &frame);
        if (ret != PROTO_OK) {
            fprintf(stderr, "[reliable] send failed on attempt %d\n", attempt + 1);
            continue;
        }

        frame_t ack;
        memset(&ack, 0, sizeof(ack));
        ret = recv_frame(conn->fd, &ack);

        if (ret != PROTO_OK) {
            fprintf(stderr, "[reliable] ack recv failed on attempt %d\n", attempt + 1);
            frame_free(&ack);
            continue;
        }

        if (ack.header.type == FRAME_ACK && ack.header.seq == conn->local_seq) {
            conn->local_seq++;
            frame_free(&ack);
            return PROTO_OK;
        }

        if (ack.header.type == FRAME_NACK) {
            fprintf(stderr, "[reliable] nack received, retrying seq=%u\n", conn->local_seq);
            frame_free(&ack);
            continue;
        }

        frame_free(&ack);
    }

    fprintf(stderr, "[reliable] max retries exhausted for seq=%u\n", conn->local_seq);
    return PROTO_ERR_IO;
}

int reliable_recv(connection_t *conn, uint8_t *buf, size_t buf_len)
{
    if (!conn || !buf)
        return PROTO_ERR_IO;

    frame_t frame;
    memset(&frame, 0, sizeof(frame));

    int ret = recv_frame(conn->fd, &frame);
    if (ret != PROTO_OK) {
        send_nack(conn->fd, conn->remote_seq);
        return ret;
    }

    if (frame.header.type != FRAME_DATA) {
        frame_free(&frame);
        return PROTO_ERR_STATE;
    }

    if (frame.header.seq != conn->remote_seq) {
        fprintf(stderr, "[reliable] seq mismatch: expected=%u got=%u\n",
                conn->remote_seq, frame.header.seq);
        send_nack(conn->fd, conn->remote_seq);
        frame_free(&frame);
        return PROTO_ERR_SEQ;
    }

    uint32_t plen = frame.header.payload_len;
    if (plen > buf_len) {
        send_nack(conn->fd, conn->remote_seq);
        frame_free(&frame);
        return PROTO_ERR_LEN;
    }

    if (plen > 0 && frame.payload)
        memcpy(buf, frame.payload, plen);

    send_ack(conn->fd, conn->remote_seq);
    conn->remote_seq++;

    frame_free(&frame);
    return (int)plen;
}

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "protocol.h"

#define SERVER_HOST  "127.0.0.1"
#define SERVER_PORT  9000
#define BUF_SIZE     4096

int main(int argc, char *argv[])
{
    const char *host = (argc > 1) ? argv[1] : SERVER_HOST;
    uint16_t    port = (argc > 2) ? (uint16_t)atoi(argv[2]) : SERVER_PORT;

    fprintf(stdout, "[client] connecting to %s:%u\n", host, port);

    int fd = transport_connect(host, port);
    if (fd < 0) {
        fprintf(stderr, "[client] connection failed\n");
        return EXIT_FAILURE;
    }

    connection_t conn;
    if (conn_init(&conn, fd) != PROTO_OK) {
        fprintf(stderr, "[client] conn_init failed\n");
        transport_close(fd);
        return EXIT_FAILURE;
    }

    if (conn_handshake_client(&conn) != PROTO_OK) {
        fprintf(stderr, "[client] handshake failed\n");
        transport_close(fd);
        return EXIT_FAILURE;
    }

    fprintf(stdout, "[client] connected. type messages (ctrl+d to quit)\n\n");

    char     line[BUF_SIZE];
    uint8_t  resp[BUF_SIZE];

    while (fgets(line, sizeof(line), stdin)) {
        size_t len = strlen(line);
        if (len == 0)
            continue;

        if (line[len - 1] == '\n')
            line[--len] = '\0';

        if (len == 0)
            continue;

        int ret = conn_send(&conn, (uint8_t *)line, len);
        if (ret != PROTO_OK) {
            fprintf(stderr, "[client] send failed: %d\n", ret);
            break;
        }

        int n = conn_recv(&conn, resp, BUF_SIZE - 1);
        if (n < 0) {
            fprintf(stderr, "[client] recv failed: %d\n", n);
            break;
        }

        resp[n] = '\0';
        fprintf(stdout, "[client] echo -> (%d bytes): %s\n", n, (char *)resp);
    }

    fprintf(stdout, "\n[client] closing connection\n");
    conn_close(&conn);
    return EXIT_SUCCESS;
}

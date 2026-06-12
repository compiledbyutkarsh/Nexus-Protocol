#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <signal.h>
#include "protocol.h"

#define LISTEN_PORT  9000
#define BUF_SIZE     4096

static volatile int running = 1;

static void handle_sig(int sig)
{
    (void)sig;
    running = 0;
}

static void handle_client(int client_fd)
{
    connection_t conn;
    uint8_t      buf[BUF_SIZE];

    if (conn_init(&conn, client_fd) != PROTO_OK) {
        fprintf(stderr, "[server] conn_init failed\n");
        transport_close(client_fd);
        return;
    }

    if (conn_handshake_server(&conn) != PROTO_OK) {
        fprintf(stderr, "[server] handshake failed\n");
        transport_close(client_fd);
        return;
    }

    fprintf(stdout, "[server] connection established, entering echo loop\n");

    while (conn.state == CONN_ESTABLISHED) {
        int n = conn_recv(&conn, buf, BUF_SIZE);
        if (n < 0) {
            fprintf(stderr, "[server] recv error: %d\n", n);
            break;
        }
        if (n == 0)
            break;

        buf[n] = '\0';
        fprintf(stdout, "[server] echo <- (%d bytes): %s\n", n, buf);

        int ret = conn_send(&conn, buf, (size_t)n);
        if (ret != PROTO_OK) {
            fprintf(stderr, "[server] send error: %d\n", ret);
            break;
        }
    }

    conn_close(&conn);
    fprintf(stdout, "[server] client disconnected\n");
}

int main(void)
{
    signal(SIGINT,  handle_sig);
    signal(SIGTERM, handle_sig);

    int server_fd = transport_listen(LISTEN_PORT);
    if (server_fd < 0) {
        fprintf(stderr, "[server] failed to bind on port %d\n", LISTEN_PORT);
        return EXIT_FAILURE;
    }

    fprintf(stdout, "[server] listening on port %d\n", LISTEN_PORT);

    while (running) {
        int client_fd = transport_accept(server_fd);
        if (client_fd < 0)
            continue;
        handle_client(client_fd);
    }

    transport_close(server_fd);
    fprintf(stdout, "[server] shutdown complete\n");
    return EXIT_SUCCESS;
}

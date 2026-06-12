#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <errno.h>
#include <fcntl.h>
#include <netdb.h>
#include <arpa/inet.h>
#include <sys/socket.h>
#include <sys/types.h>
#include "protocol.h"


static int set_sockopts(int fd)
{
    int yes = 1;
    if (setsockopt(fd, SOL_SOCKET, SO_REUSEADDR, &yes, sizeof(yes)) < 0)
        return PROTO_ERR_IO;
    if (setsockopt(fd, SOL_SOCKET, SO_KEEPALIVE, &yes, sizeof(yes)) < 0)
        return PROTO_ERR_IO;
    return PROTO_OK;
}

int transport_connect(const char *host, uint16_t port)
{
    struct addrinfo hints, *res, *rp;
    char port_str[6];
    int fd = -1;

    memset(&hints, 0, sizeof(hints));
    hints.ai_family   = AF_UNSPEC;
    hints.ai_socktype = SOCK_STREAM;

    snprintf(port_str, sizeof(port_str), "%u", port);

    if (getaddrinfo(host, port_str, &hints, &res) != 0)
        return PROTO_ERR_IO;

    for (rp = res; rp; rp = rp->ai_next) {
        fd = socket(rp->ai_family, rp->ai_socktype, rp->ai_protocol);
        if (fd < 0)
            continue;
        if (set_sockopts(fd) != PROTO_OK) {
            close(fd);
            continue;
        }
        if (connect(fd, rp->ai_addr, rp->ai_addrlen) == 0)
            break;
        close(fd);
        fd = -1;
    }

    freeaddrinfo(res);

    if (fd < 0)
        return PROTO_ERR_IO;

    return fd;
}

int transport_listen(uint16_t port)
{
    int fd;
    struct sockaddr_in addr;

    fd = socket(AF_INET, SOCK_STREAM, 0);
    if (fd < 0)
        return PROTO_ERR_IO;

    if (set_sockopts(fd) != PROTO_OK) {
        close(fd);
        return PROTO_ERR_IO;
    }

    memset(&addr, 0, sizeof(addr));
    addr.sin_family      = AF_INET;
    addr.sin_addr.s_addr = INADDR_ANY;
    addr.sin_port        = htons(port);

    if (bind(fd, (struct sockaddr *)&addr, sizeof(addr)) < 0) {
        close(fd);
        return PROTO_ERR_IO;
    }

    if (listen(fd, 128) < 0) {
        close(fd);
        return PROTO_ERR_IO;
    }

    return fd;
}

int transport_accept(int server_fd)
{
    struct sockaddr_storage peer;
    socklen_t peer_len = sizeof(peer);
    int fd;

    fd = accept(server_fd, (struct sockaddr *)&peer, &peer_len);
    if (fd < 0)
        return PROTO_ERR_IO;

    char host[INET6_ADDRSTRLEN];
    uint16_t prt = 0;

    if (peer.ss_family == AF_INET) {
        struct sockaddr_in *s = (struct sockaddr_in *)&peer;
        inet_ntop(AF_INET, &s->sin_addr, host, sizeof(host));
        prt = ntohs(s->sin_port);
    } else {
        struct sockaddr_in6 *s = (struct sockaddr_in6 *)&peer;
        inet_ntop(AF_INET6, &s->sin6_addr, host, sizeof(host));
        prt = ntohs(s->sin6_port);
    }

    fprintf(stdout, "[transport] accepted connection from %s:%u\n", host, prt);
    return fd;
}

ssize_t transport_send(int fd, const uint8_t *buf, size_t len)
{
    size_t  sent = 0;
    ssize_t n;

    while (sent < len) {
        n = send(fd, buf + sent, len - sent, MSG_NOSIGNAL);
        if (n < 0) {
            if (errno == EAGAIN || errno == EWOULDBLOCK)
                continue;
            return PROTO_ERR_IO;
        }
        sent += (size_t)n;
    }

    return (ssize_t)sent;
}

ssize_t transport_recv(int fd, uint8_t *buf, size_t len)
{
    size_t  recvd = 0;
    ssize_t n;

    while (recvd < len) {
        n = recv(fd, buf + recvd, len - recvd, 0);
        if (n < 0) {
            if (errno == EAGAIN || errno == EWOULDBLOCK)
                continue;
            return PROTO_ERR_IO;
        }
        if (n == 0)
            return PROTO_ERR_IO;
        recvd += (size_t)n;
    }

    return (ssize_t)recvd;
}

void transport_close(int fd)
{
    if (fd >= 0)
        close(fd);
}

#include <sys/socket.h>
#include <arpa/inet.h>
#include <string.h>
#include <unistd.h>
#include <fcntl.h>
#include <errno.h>
#include <stdio.h>

#include "telnet.h"

static int sock = -1;

int telnet_open() {
    struct sockaddr_in server;
    int flags;
    int rs;

    sock = socket(AF_INET, SOCK_STREAM, 0);
    if (sock == -1) {
        return -1;
    }

    memset(&server, 0, sizeof(server));

    server.sin_family = AF_INET;
    server.sin_port = htons(9000);

    if (inet_pton(AF_INET, "127.0.0.1", &server.sin_addr) <= 0) {
        shutdown(sock, 2);
        close(sock);
        sock = -1;
        return -1;
    }

    if ((rs = connect(sock, (struct sockaddr *)&server, sizeof(server))) < 0) {
        printf("[telnet_open] telnet connect failed. rs=%d\n", rs);
        shutdown(sock, 2);
        close(sock);
        sock = -1;
        return -1;
    }

    flags = fcntl(sock, F_GETFL, 0);
    rs = fcntl(sock, F_SETFL, flags | O_NONBLOCK);
    if (rs) {
        printf("[telnet_open] Setting O_NONBLOCK failed!\n");
    }

    return 0;
}

int telnet_close() {
    if (sock < 0) {
        return -1;
    }

    shutdown(sock, 2);
    close(sock);
    sock = -1;

    return 0;
}

ssize_t telnet_read(uint8_t *buf, size_t len) {
    int result;

    if (sock < 0) {
        return -1;
    }

    result = read(sock, buf, len);

    if (result == -1) {
        if (errno == EAGAIN) {
            return 0;
        } else {
            printf("[telnet_read] errno = %d\n", errno);
            return -1;
        }
    } else {
        return result;
    }
}

ssize_t telnet_send(uint8_t* buf, size_t len) {
    int result;

    result = send(sock, buf, len, MSG_DONTWAIT);

    if (result == -1) {
        if (errno == EAGAIN) {
            return 0;
        } else {
            printf("[telnet_send] errno = %d\n", errno);
            return -1;
        }
    } else {
        return result;
    }
}

#include "ae.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <fcntl.h>
#include <sys/un.h>
#include <sys/socket.h>
#include <errno.h>
#include "http_server.h"
#include "flow_control.h"
#include "logging.h"
#include "metrics.h"

#define SOCKET_PATH "/tmp/http.sock"
#define MAX_CONCURRENCY 1000

aeEventLoop *loop;
int server_fd;

void on_accept(aeEventLoop *el, int fd, void *privdata, int mask) {
    (void)el; (void)privdata; (void)mask;
    struct sockaddr_un sock_client;
    socklen_t client_len = sizeof(sock_client);
    int cfd = accept(fd, (struct sockaddr*)&sock_client, &client_len);
    if (cfd == -1) return;

    if (g_metrics.active_connections >= MAX_CONCURRENCY) {
        close(cfd);
        return;
    }

    // Set non-blocking
    int flags = fcntl(cfd, F_GETFL, 0);
    fcntl(cfd, F_SETFL, flags | O_NONBLOCK);

    http_server_accept_connection(el, cfd);
}

int main() {
    metrics_init();
    loop = aeCreateEventLoop(MAX_CONCURRENCY + 128);
    if (!loop) {
        log_error("Failed to create aeEventLoop");
        return 1;
    }

    // 1. Initialize Flow Control
    flow_control_init(loop);

    // 2. Setup Unix Domain Socket Server Pipe
    unlink(SOCKET_PATH);
    server_fd = socket(AF_UNIX, SOCK_STREAM, 0);
    if (server_fd == -1) {
        log_error("socket error: %s", strerror(errno));
        return 1;
    }

    int flags = fcntl(server_fd, F_GETFL, 0);
    fcntl(server_fd, F_SETFL, flags | O_NONBLOCK);

    struct sockaddr_un saddr;
    memset(&saddr, 0, sizeof(saddr));
    saddr.sun_family = AF_UNIX;
    strncpy(saddr.sun_path, SOCKET_PATH, sizeof(saddr.sun_path)-1);

    if (bind(server_fd, (struct sockaddr*)&saddr, sizeof(saddr)) == -1) {
        log_error("bind error: %s", strerror(errno));
        return 1;
    }

    if (listen(server_fd, 511) == -1) {
        log_error("listen error: %s", strerror(errno));
        return 1;
    }

    if (aeCreateFileEvent(loop, server_fd, AE_READABLE, on_accept, NULL) == AE_ERR) {
        log_error("Failed to create ae file event");
        return 1;
    }

    log_info("Server listening on Unix Socket: %s", SOCKET_PATH);

    // 3. Start Event Loop
    aeMain(loop);
    aeDeleteEventLoop(loop);
    return 0;
}

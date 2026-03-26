#include <uv.h>
#include <stdio.h>
#include <stdlib.h>
#include "http_server.h"
#include "flow_control.h"

#define SOCKET_PATH "/tmp/http.sock"
#define MAX_CONCURRENCY 1000

uv_loop_t *loop;
uv_pipe_t server_pipe;
int active_connections = 0;

void on_new_connection(uv_stream_t *server, int status) {
    if (status < 0) {
        fprintf(stderr, "New connection error %s\n", uv_strerror(status));
        return;
    }

    if (active_connections >= MAX_CONCURRENCY) {
        // Here we could simply not accept() until we dip below limit
        // But to properly refuse, we must accept and eagerly close.
        uv_pipe_t *dummy = malloc(sizeof(uv_pipe_t));
        uv_pipe_init(loop, dummy, 0);
        if (uv_accept(server, (uv_stream_t*)dummy) == 0) {
            uv_close((uv_handle_t*)dummy, (uv_close_cb)free);
        } else {
            free(dummy);
        }
        return;
    }

    // Accept real connection and hand it off to the http_server state machine
    http_server_accept_connection(server);
}

int main() {
    loop = uv_default_loop();

    // 1. Initialize Flow Control
    flow_control_init(loop);

    // 2. Setup Unix Domain Socket Server Pipe
    uv_pipe_init(loop, &server_pipe, 0);

    // Unlink old socket just in case
    uv_fs_t req;
    uv_fs_unlink(loop, &req, SOCKET_PATH, NULL);
    uv_fs_req_cleanup(&req);

    int r;
    if ((r = uv_pipe_bind(&server_pipe, SOCKET_PATH))) {
        fprintf(stderr, "Bind error %s\n", uv_err_name(r));
        return 1;
    }

    // Listen
    if ((r = uv_listen((uv_stream_t*)&server_pipe, 128, on_new_connection))) {
        fprintf(stderr, "Listen error %s\n", uv_err_name(r));
        return 1;
    }

    printf("Server listening on Unix Socket: %s\n", SOCKET_PATH);

    // 3. Start Event Loop
    return uv_run(loop, UV_RUN_DEFAULT);
}

#ifndef HTTP_SERVER_H
#define HTTP_SERVER_H

#include <uv.h>
#include <llhttp.h>
#include <stdbool.h>

// Represents an active connection state
typedef struct client_t {
    uv_tcp_t handle; // technically uv_pipe_t, but uv_stream_t works for both
    llhttp_t parser;
    llhttp_settings_t parser_settings;

    // Buffer for building the request body (JSON)
    char *body;
    size_t body_len;

    // Parsed info
    bool has_auth_header;
    bool auth_valid;
    bool accept_lz4;
    bool accept_deflate;

    // Helper context strings
    char *current_header_field;

} client_t;

// Invoked from main.c when a new connection is accepted
void http_server_accept_connection(uv_stream_t *server);

#endif // HTTP_SERVER_H

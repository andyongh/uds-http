#ifndef HTTP_SERVER_H
#define HTTP_SERVER_H

#include "ae.h"
#include "llhttp.h"
#include <stdbool.h>

// Represents an active connection state
typedef struct client_t {
    aeEventLoop *loop;
    int fd;

    llhttp_t parser;
    llhttp_settings_t parser_settings;

    // Request state
    char *url;
    char *body;
    size_t body_len;
    bool has_auth_header;
    bool auth_valid;
    bool accept_lz4;
    bool accept_deflate;
    char *current_header_field;

    // Response Write state
    char *write_buf;
    size_t write_len;
    size_t write_pos;
    bool keep_alive;     // True if the current response should keep connection open
    bool is_closing;     // Marked if a close is requested after write

} client_t;

// Invoked from main.c when a new connection is accepted
void http_server_accept_connection(aeEventLoop *loop, int cfd);

#endif // HTTP_SERVER_H

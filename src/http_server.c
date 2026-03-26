#include "http_server.h"
#include "flow_control.h"
#include "auth.h"
#include "json_handler.h"
#include "compression.h"
#include <stdlib.h>
#include <string.h>
#include <stdio.h>
#include <ctype.h>

extern int active_connections;

static void free_client(uv_handle_t *handle) {
    client_t *client = (client_t *)handle->data;
    if (client) {
        if (client->body) free(client->body);
        if (client->current_header_field) free(client->current_header_field);
        free(client);
        active_connections--;
    }
}

struct write_req_t {
    uv_write_t uv_req;
    char *buf;
};

static void on_custom_write_end(uv_write_t *r, int st) {
    (void)st;
    struct write_req_t *wr = (struct write_req_t *)r;
    client_t *c = (client_t *)r->data;
    if (wr->buf) free(wr->buf);
    free(wr);
    
    // Check keep-alive
    if (llhttp_should_keep_alive(&c->parser)) {
        // Reset state for next request
        // llhttp_reset is not fully complete, better to just call llhttp_init again ?
        // llhttp_reset(&c->parser) is adequate for pipelining if callbacks don't assume otherwise
        // But our on_message_begin also resets custom state!
        // So we just need to ensure llhttp is ready.
        // wait, llhttp handles keep-alive automatically by moving to HPE_OK state.
    } else {
        if (!uv_is_closing((uv_handle_t*)&c->handle)) {
            uv_close((uv_handle_t*)&c->handle, free_client);
        }
    }
}

static void send_response(client_t *client, int status_code, const char *status_text, const char *body, size_t body_len, compression_type_t comp_type) {
    char header_buf[512];
    const char *encoding = "";
    if (comp_type == COMPRESSION_LZ4) encoding = "\r\nContent-Encoding: lz4";
    else if (comp_type == COMPRESSION_DEFLATE) encoding = "\r\nContent-Encoding: deflate";

    const char *conn_header = llhttp_should_keep_alive(&client->parser) ? "keep-alive" : "close";

    int header_len = snprintf(header_buf, sizeof(header_buf), 
        "HTTP/1.1 %d %s\r\n"
        "Connection: %s\r\n"
        "Content-Type: application/json%s\r\n"
        "Content-Length: %zu\r\n\r\n", 
        status_code, status_text, conn_header, encoding, body_len);

    size_t total_len = header_len + body_len;
    char *full_response = malloc(total_len);
    memcpy(full_response, header_buf, header_len);
    if (body && body_len > 0) {
        memcpy(full_response + header_len, body, body_len);
    }

    struct write_req_t *wreq = malloc(sizeof(struct write_req_t));
    wreq->uv_req.data = client;
    wreq->buf = full_response;

    uv_buf_t buf = uv_buf_init(full_response, total_len);
    uv_write(&wreq->uv_req, (uv_stream_t*)&client->handle, &buf, 1, on_custom_write_end);
}

// llhttp callbacks
static int on_message_begin(llhttp_t* parser) {
    client_t *client = parser->data;
    if (client->body) free(client->body);
    client->body = NULL;
    client->body_len = 0;
    client->has_auth_header = false;
    client->auth_valid = false;
    client->accept_lz4 = false;
    client->accept_deflate = false;
    if (client->current_header_field) free(client->current_header_field);
    client->current_header_field = NULL;
    return 0;
}

static int on_header_field(llhttp_t* parser, const char* at, size_t length) {
    client_t *client = parser->data;
    if (client->current_header_field) free(client->current_header_field);
    client->current_header_field = malloc(length + 1);
    memcpy(client->current_header_field, at, length);
    client->current_header_field[length] = '\0';
    for (size_t i = 0; i < length; i++) {
        client->current_header_field[i] = tolower((unsigned char)client->current_header_field[i]);
    }
    return 0;
}

static int on_header_value(llhttp_t* parser, const char* at, size_t length) {
    client_t *client = parser->data;
    if (!client->current_header_field) return 0;

    if (strcmp(client->current_header_field, "authorization") == 0) {
        client->has_auth_header = true;
        client->auth_valid = auth_check_token(at, length);
    } else if (strcmp(client->current_header_field, "accept-encoding") == 0) {
        // Very basic substring search for accepted encodings
        char *val = malloc(length + 1);
        memcpy(val, at, length);
        val[length] = '\0';
        for (size_t i = 0; i < length; i++) val[i] = tolower((unsigned char)val[i]);
        if (strstr(val, "lz4")) client->accept_lz4 = true;
        if (strstr(val, "deflate")) client->accept_deflate = true;
        free(val);
    }
    return 0;
}

static int on_body(llhttp_t* parser, const char* at, size_t length) {
    client_t *client = parser->data;
    if (!client->body) {
        client->body = malloc(length);
        memcpy(client->body, at, length);
        client->body_len = length;
    } else {
        char *new_body = realloc(client->body, client->body_len + length);
        memcpy(new_body + client->body_len, at, length);
        client->body = new_body;
        client->body_len += length;
    }
    return 0;
}

static int on_message_complete(llhttp_t* parser) {
    client_t *client = parser->data;

    // 1. Flow Control Check
    if (!flow_control_consume_token()) {
        const char *err = "{\"error\": \"Too Many Requests\"}";
        send_response(client, 429, "Too Many Requests", err, strlen(err), COMPRESSION_NONE);
        return 0; // Pause parser, connection will close
    }

    // 2. Auth Check
    if (!client->has_auth_header || !client->auth_valid) {
        const char *err = "{\"error\": \"Unauthorized\"}";
        send_response(client, 401, "Unauthorized", err, strlen(err), COMPRESSION_NONE);
        return 0;
    }

    // 3. Process JSON
    size_t out_json_len = 0;
    char *out_json = json_handler_process(client->body, client->body_len, &out_json_len);

    // 4. Compression (preference order: lz4, deflate, none)
    compression_type_t comp_type = COMPRESSION_NONE;
    if (client->accept_lz4) comp_type = COMPRESSION_LZ4;
    else if (client->accept_deflate) comp_type = COMPRESSION_DEFLATE;

    size_t comp_out_len = 0;
    char *comp_payload = compress_payload(comp_type, out_json, out_json_len, &comp_out_len);
    
    // 5. Send Response
    send_response(client, 200, "OK", comp_payload ? comp_payload : out_json, 
                  comp_payload ? comp_out_len : out_json_len, 
                  comp_payload ? comp_type : COMPRESSION_NONE);

    if (out_json) free(out_json);
    if (comp_payload) free(comp_payload);

    return 0;
}

// libuv read callbacks
static void on_alloc(uv_handle_t *handle, size_t suggested_size, uv_buf_t *buf) {
    (void)handle;
    *buf = uv_buf_init(malloc(suggested_size), suggested_size);
}

static void on_read(uv_stream_t *stream, ssize_t nread, const uv_buf_t *buf) {
    client_t *client = (client_t *)stream->data;
    
    if (nread > 0) {
        enum llhttp_errno err = llhttp_execute(&client->parser, buf->base, nread);
        if (err != HPE_OK) {
            fprintf(stderr, "llhttp parse error: %s %s\n", llhttp_errno_name(err), client->parser.reason);
            if (!uv_is_closing((uv_handle_t*)&client->handle)) {
                uv_close((uv_handle_t*)&client->handle, free_client);
            }
        }
    } else if (nread < 0) {
        if (nread != UV_EOF) {
            fprintf(stderr, "Read error %s\n", uv_err_name(nread));
        }
        if (!uv_is_closing((uv_handle_t*)&client->handle)) {
            uv_close((uv_handle_t*)&client->handle, free_client);
        }
    }
    
    if (buf->base) free(buf->base);
}

void http_server_accept_connection(uv_stream_t *server) {
    client_t *client = calloc(1, sizeof(client_t));
    client->handle.data = client; // cross reference for uv_handle

    uv_tcp_init(server->loop, &client->handle);
    if (uv_accept(server, (uv_stream_t*)&client->handle) == 0) {
        active_connections++;
        
        // Initialize parser
        llhttp_settings_init(&client->parser_settings);
        client->parser_settings.on_message_begin = on_message_begin;
        client->parser_settings.on_header_field = on_header_field;
        client->parser_settings.on_header_value = on_header_value;
        client->parser_settings.on_body = on_body;
        client->parser_settings.on_message_complete = on_message_complete;
        
        llhttp_init(&client->parser, HTTP_REQUEST, &client->parser_settings);
        client->parser.data = client; // Give parser ref to client context
        
        uv_read_start((uv_stream_t*)&client->handle, on_alloc, on_read);
    } else {
        uv_close((uv_handle_t*)&client->handle, free_client);
    }
}

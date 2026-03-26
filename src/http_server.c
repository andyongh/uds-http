#include "http_server.h"
#include "flow_control.h"
#include "auth.h"
#include "json_handler.h"
#include "compression.h"
#include "logging.h"
#include "metrics.h"
#include "yyjson.h"
#include <stdlib.h>
#include <string.h>
#include <stdio.h>
#include <ctype.h>
#include <unistd.h>
#include <errno.h>

static void free_client(client_t *client) {
    if (!client) return;
    aeDeleteFileEvent(client->loop, client->fd, AE_READABLE | AE_WRITABLE);
    close(client->fd);

    if (client->body) free(client->body);
    if (client->url) free(client->url);
    if (client->current_header_field) free(client->current_header_field);
    if (client->write_buf) free(client->write_buf);
    free(client);
    if (g_metrics.active_connections > 0) g_metrics.active_connections--;
}

static void on_write_ready(aeEventLoop *el, int fd, void *privdata, int mask) {
    (void)el; (void)mask;
    client_t *client = (client_t *)privdata;

    if (client->write_len > client->write_pos) {
        ssize_t nwritten = write(fd, client->write_buf + client->write_pos, client->write_len - client->write_pos);
        if (nwritten < 0) {
            if (errno == EAGAIN || errno == EWOULDBLOCK) {
                return; // wait for next writable event
            }
            free_client(client);
            return;
        }
        client->write_pos += nwritten;
        g_metrics.bytes_written += nwritten;
    }

    // Done writing
    if (client->write_pos == client->write_len) {
        aeDeleteFileEvent(client->loop, client->fd, AE_WRITABLE); // Remove write watch
        free(client->write_buf);
        client->write_buf = NULL;
        client->write_len = 0;
        client->write_pos = 0;

        if (client->is_closing || !client->keep_alive) {
            free_client(client);
        }
    }
}

static void enqueue_write(client_t *client, char *data, size_t len) {
    if (!data || len == 0) return;

    if (!client->write_buf) {
        client->write_buf = malloc(len);
        memcpy(client->write_buf, data, len);
        client->write_len = len;
        client->write_pos = 0;
    } else {
        char *new_buf = realloc(client->write_buf, client->write_len + len);
        memcpy(new_buf + client->write_len, data, len);
        client->write_buf = new_buf;
        client->write_len += len;
    }

    // Attempt inline synchronous write first to save epoll syscalls
    ssize_t nwritten = write(client->fd, client->write_buf + client->write_pos, client->write_len - client->write_pos);
    if (nwritten > 0) {
        client->write_pos += nwritten;
        g_metrics.bytes_written += nwritten;
        if (client->write_pos == client->write_len) {
            free(client->write_buf);
            client->write_buf = NULL;
            client->write_len = 0;
            client->write_pos = 0;
            if (client->is_closing || !client->keep_alive) {
                free_client(client);
                return;
            }
            return; // Fully written synchronously!
        }
    }

    // If partial write or EAGAIN, let on_write_ready handle it
    aeCreateFileEvent(client->loop, client->fd, AE_WRITABLE, on_write_ready, client);
}

static void send_response(client_t *client, int status_code, const char *status_text, const char *body, size_t body_len, compression_type_t comp_type) {
    char header_buf[512];
    const char *encoding = "";
    if (comp_type == COMPRESSION_LZ4) encoding = "\r\nContent-Encoding: lz4";
    else if (comp_type == COMPRESSION_DEFLATE) encoding = "\r\nContent-Encoding: deflate";

    client->keep_alive = llhttp_should_keep_alive(&client->parser);
    const char *conn_header = client->keep_alive ? "keep-alive" : "close";

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

    enqueue_write(client, full_response, total_len);
    free(full_response);
}

// llhttp callbacks
static int on_message_begin(llhttp_t* parser) {
    client_t *client = parser->data;
    if (client->body) free(client->body);
    if (client->url) free(client->url);
    client->body = NULL;
    client->url = NULL;
    client->body_len = 0;
    client->has_auth_header = false;
    client->auth_valid = false;
    client->accept_lz4 = false;
    client->accept_deflate = false;
    if (client->current_header_field) free(client->current_header_field);
    client->current_header_field = NULL;
    return 0;
}

static int on_url(llhttp_t* parser, const char* at, size_t length) {
    client_t *client = parser->data;
    if (!client->url) {
        client->url = malloc(length + 1);
        memcpy(client->url, at, length);
        client->url[length] = '\0';
    } else {
        size_t old_len = strlen(client->url);
        char *new_url = realloc(client->url, old_len + length + 1);
        memcpy(new_url + old_len, at, length);
        new_url[old_len + length] = '\0';
        client->url = new_url;
    }
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

static void serve_metrics_yyjson(client_t *client) {
    yyjson_mut_doc *doc = yyjson_mut_doc_new(NULL);
    yyjson_mut_val *obj = yyjson_mut_obj(doc);
    yyjson_mut_doc_set_root(doc, obj);
    yyjson_mut_obj_add_uint(doc, obj, "total_requests", g_metrics.total_requests);
    yyjson_mut_obj_add_uint(doc, obj, "bytes_read", g_metrics.bytes_read);
    yyjson_mut_obj_add_uint(doc, obj, "bytes_written", g_metrics.bytes_written);
    yyjson_mut_obj_add_uint(doc, obj, "active_connections", g_metrics.active_connections);
    yyjson_mut_obj_add_uint(doc, obj, "rate_limit_drops", g_metrics.rate_limit_drops);
    
    size_t len;
    char *json = yyjson_mut_write(doc, 0, &len);
    send_response(client, 200, "OK", json, len, COMPRESSION_NONE);
    free(json);
    yyjson_mut_doc_free(doc);
}

static void serve_config_yyjson(client_t *client) {
    yyjson_mut_doc *doc = yyjson_mut_doc_new(NULL);
    yyjson_mut_val *obj = yyjson_mut_obj(doc);
    yyjson_mut_doc_set_root(doc, obj);
    yyjson_mut_obj_add_uint(doc, obj, "rps_limit", 50000);
    yyjson_mut_obj_add_uint(doc, obj, "bucket_capacity", 50000);
    yyjson_mut_obj_add_uint(doc, obj, "max_concurrency", 1000);
    
    size_t len;
    char *json = yyjson_mut_write(doc, 0, &len);
    send_response(client, 200, "OK", json, len, COMPRESSION_NONE);
    free(json);
    yyjson_mut_doc_free(doc);
}

static int on_message_complete(llhttp_t* parser) {
    client_t *client = parser->data;

    // Flow Control
    if (!flow_control_consume_token()) {
        g_metrics.rate_limit_drops++;
        const char *err = "{\"error\": \"Too Many Requests\"}";
        send_response(client, 429, "Too Many Requests", err, strlen(err), COMPRESSION_NONE);
        return 0;
    }

    g_metrics.total_requests++;

    // Auth
    if (!client->has_auth_header || !client->auth_valid) {
        const char *err = "{\"error\": \"Unauthorized\"}";
        send_response(client, 401, "Unauthorized", err, strlen(err), COMPRESSION_NONE);
        return 0;
    }

    // Routing
    if (client->url) {
        if (strcmp(client->url, "/metrics") == 0) {
            serve_metrics_yyjson(client);
            return 0;
        } else if (strcmp(client->url, "/config") == 0) {
            serve_config_yyjson(client);
            return 0;
        }
    }

    // Process JSON (Default action)
    size_t out_json_len = 0;
    char *out_json = json_handler_process(client->body, client->body_len, &out_json_len);

    // Compress
    compression_type_t comp_type = COMPRESSION_NONE;
    if (client->accept_lz4) comp_type = COMPRESSION_LZ4;
    else if (client->accept_deflate) comp_type = COMPRESSION_DEFLATE;

    size_t comp_out_len = 0;
    char *comp_payload = compress_payload(comp_type, out_json, out_json_len, &comp_out_len);
    
    send_response(client, 200, "OK", comp_payload ? comp_payload : out_json, 
                  comp_payload ? comp_out_len : out_json_len, 
                  comp_payload ? comp_type : COMPRESSION_NONE);

    if (out_json) free(out_json);
    if (comp_payload) free(comp_payload);

    return 0; // llhttp will safely transition to processing pipelined requests
}

static void on_read_ready(aeEventLoop *el, int fd, void *privdata, int mask) {
    (void)el; (void)mask;
    client_t *client = (client_t *)privdata;
    char buf[8192];

    while (1) {
        ssize_t nread = read(fd, buf, sizeof(buf));
        if (nread < 0) {
            if (errno == EAGAIN || errno == EWOULDBLOCK) {
                break; // done reading all available buffers
            }
            free_client(client);
            return;
        } else if (nread == 0) {
            free_client(client); // EOF
            return;
        }

        g_metrics.bytes_read += nread;

        enum llhttp_errno err = llhttp_execute(&client->parser, buf, nread);
        if (err != HPE_OK) {
            log_error("llhttp parse error: %s %s", llhttp_errno_name(err), client->parser.reason);
            free_client(client);
            return;
        }
    }
}

void http_server_accept_connection(aeEventLoop *loop, int cfd) {
    client_t *client = calloc(1, sizeof(client_t));
    client->loop = loop;
    client->fd = cfd;
    g_metrics.active_connections++;

    llhttp_settings_init(&client->parser_settings);
    client->parser_settings.on_message_begin = on_message_begin;
    client->parser_settings.on_url = on_url;
    client->parser_settings.on_header_field = on_header_field;
    client->parser_settings.on_header_value = on_header_value;
    client->parser_settings.on_body = on_body;
    client->parser_settings.on_message_complete = on_message_complete;
    
    llhttp_init(&client->parser, HTTP_REQUEST, &client->parser_settings);
    client->parser.data = client;

    // Follow standard ae File events
    aeCreateFileEvent(loop, cfd, AE_READABLE, on_read_ready, client);
}

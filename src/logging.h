#ifndef LOGGING_H
#define LOGGING_H

typedef enum {
    LOG_LEVEL_INFO,
    LOG_LEVEL_WARN,
    LOG_LEVEL_ERROR
} log_level_t;

void server_log(log_level_t level, const char *fmt, ...);

#define log_info(...)  server_log(LOG_LEVEL_INFO, __VA_ARGS__)
#define log_warn(...)  server_log(LOG_LEVEL_WARN, __VA_ARGS__)
#define log_error(...) server_log(LOG_LEVEL_ERROR, __VA_ARGS__)

#endif

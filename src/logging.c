#include "logging.h"
#include "monotonic.h"
#include <stdio.h>
#include <stdarg.h>

void server_log(log_level_t level, const char *fmt, ...) {
    const char *level_str = "INFO";
    switch (level) {
        case LOG_LEVEL_INFO: level_str = "INFO"; break;
        case LOG_LEVEL_WARN: level_str = "WARN"; break;
        case LOG_LEVEL_ERROR: level_str = "ERROR"; break;
    }

    monotime now_ms = getMonotonicUs() / 1000;

    fprintf(stderr, "[%lld] [%s] ", now_ms, level_str);
    va_list args;
    va_start(args, fmt);
    vfprintf(stderr, fmt, args);
    va_end(args);
    fprintf(stderr, "\n");
}

#ifndef METRICS_H
#define METRICS_H

#include <stdint.h>

typedef struct {
    uint64_t total_requests;
    uint64_t bytes_read;
    uint64_t bytes_written;
    uint32_t active_connections;
    uint32_t rate_limit_drops;
} server_metrics_t;

extern server_metrics_t g_metrics;

void metrics_init(void);

#endif

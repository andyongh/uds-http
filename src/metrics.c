#include "metrics.h"

server_metrics_t g_metrics;

void metrics_init(void) {
    g_metrics.total_requests = 0;
    g_metrics.bytes_read = 0;
    g_metrics.bytes_written = 0;
    g_metrics.active_connections = 0;
    g_metrics.rate_limit_drops = 0;
}

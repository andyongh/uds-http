#include "flow_control.h"

// Token Bucket configuration
#define BUCKET_CAPACITY 50000
#define TOKENS_PER_MS   50    // 50,000 per second = 50 per ms

static long tokens = BUCKET_CAPACITY;
static uv_timer_t timer_req;

static void on_timer(uv_timer_t *handle) {
    (void)handle; // unused
    // Refill tokens
    tokens += TOKENS_PER_MS * 10; // since we run every 10ms
    if (tokens > BUCKET_CAPACITY) {
        tokens = BUCKET_CAPACITY;
    }
}

void flow_control_init(uv_loop_t *loop) {
    uv_timer_init(loop, &timer_req);
    // Periodically run every 10ms to refill bucket
    uv_timer_start(&timer_req, on_timer, 10, 10);
}

bool flow_control_consume_token(void) {
    if (tokens > 0) {
        tokens--;
        return true;
    }
    return false;
}

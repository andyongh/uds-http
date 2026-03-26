#include "flow_control.h"

// Token Bucket configuration
#define BUCKET_CAPACITY 50000
#define TOKENS_PER_MS   50    // 50,000 per second = 50 per ms

static long tokens = BUCKET_CAPACITY;

static long long on_timer(struct aeEventLoop *eventLoop, long long id, void *clientData) {
    (void)eventLoop; (void)id; (void)clientData;
    tokens += TOKENS_PER_MS * 10;
    if (tokens > BUCKET_CAPACITY) {
        tokens = BUCKET_CAPACITY;
    }
    return 10; // ae runs the timer again in 10ms
}

void flow_control_init(aeEventLoop *loop) {
    aeCreateTimeEvent(loop, 10, on_timer, NULL, NULL);
}

bool flow_control_consume_token(void) {
    if (tokens > 0) {
        tokens--;
        return true;
    }
    return false;
}

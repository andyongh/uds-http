#ifndef FLOW_CONTROL_H
#define FLOW_CONTROL_H

#include <uv.h>
#include <stdbool.h>

// Initializes the flow control timer on the main loop
void flow_control_init(uv_loop_t *loop);

// Consumes 1 token from the bucket. Returns true if successful.
// Returns false if the bucket is empty (i.e. too many requests).
bool flow_control_consume_token(void);

#endif // FLOW_CONTROL_H

#ifndef FLOW_CONTROL_H
#define FLOW_CONTROL_H

#include "ae.h"
#include <stdbool.h>

void flow_control_init(aeEventLoop *loop);
bool flow_control_consume_token(void);

#endif

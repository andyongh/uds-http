#ifndef MONOTONIC_H
#define MONOTONIC_H

#include <sys/time.h>
#include <stdint.h>
#include <stddef.h>

typedef long long monotime;

static inline monotime getMonotonicUs(void) {
    struct timeval tv;
    gettimeofday(&tv, NULL);
    return ((monotime)tv.tv_sec)*1000000 + tv.tv_usec;
}

static inline void monotonicInit(void) {}

#endif

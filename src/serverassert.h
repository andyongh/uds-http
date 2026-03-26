#ifndef SERVERASSERT_H
#define SERVERASSERT_H
#include <assert.h>
#include <stdio.h>
#include <stdlib.h>

#define panic(fmt, ...) do { \
    fprintf(stderr, "PANIC: " fmt "\n", ##__VA_ARGS__); \
    abort(); \
} while(0)

#endif

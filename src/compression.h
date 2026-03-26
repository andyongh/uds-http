#ifndef COMPRESSION_H
#define COMPRESSION_H

#include <stddef.h>

typedef enum {
    COMPRESSION_NONE,
    COMPRESSION_LZ4,
    COMPRESSION_DEFLATE
} compression_type_t;

// Returns malloc-ed compressed buffer, out_len is set
char* compress_payload(compression_type_t type, const char *input, size_t input_len, size_t *out_len);

#endif

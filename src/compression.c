#include "compression.h"
#include <lz4.h>
#include <zlib.h>
#include <stdlib.h>
#include <string.h>
#include <stdio.h>

char* compress_payload(compression_type_t type, const char *input, size_t input_len, size_t *out_len) {
    if (type == COMPRESSION_NONE) {
        *out_len = input_len;
        char *copy = malloc(input_len);
        memcpy(copy, input, input_len);
        return copy;
    }

    if (type == COMPRESSION_LZ4) {
        int max_dst_size = LZ4_compressBound(input_len);
        char *out = malloc(max_dst_size);
        int compressed_size = LZ4_compress_default(input, out, input_len, max_dst_size);
        if (compressed_size <= 0) {
            free(out);
            *out_len = 0;
            return NULL;
        }
        *out_len = compressed_size;
        return out;
    }

    if (type == COMPRESSION_DEFLATE) {
        z_stream zs;
        memset(&zs, 0, sizeof(zs));
        if (deflateInit(&zs, Z_DEFAULT_COMPRESSION) != Z_OK) {
            *out_len = 0;
            return NULL;
        }
        
        zs.next_in = (Bytef*)input;
        zs.avail_in = input_len;
        
        // maximum bound
        size_t max_out = deflateBound(&zs, input_len);
        char *out = malloc(max_out);
        
        zs.next_out = (Bytef*)out;
        zs.avail_out = max_out;
        
        int ret = deflate(&zs, Z_FINISH);
        if (ret != Z_STREAM_END) {
            free(out);
            deflateEnd(&zs);
            *out_len = 0;
            return NULL;
        }
        
        *out_len = zs.total_out;
        deflateEnd(&zs);
        return out;
    }

    *out_len = 0;
    return NULL;
}

#ifndef JSON_HANDLER_H
#define JSON_HANDLER_H

#include <stddef.h>

// Process the incoming JSON body and return a response JSON string.
// out_len will be set to the length of the returned string.
// The caller is responsible for freeing the return value.
char* json_handler_process(const char *body, size_t body_len, size_t *out_len);

#endif

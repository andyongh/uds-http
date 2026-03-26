#ifndef AUTH_H
#define AUTH_H

#include <stdbool.h>
#include <stddef.h>

// Checks if the given header value matches our token
bool auth_check_token(const char *header_value, size_t len);

#endif

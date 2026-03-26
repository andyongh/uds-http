#include "auth.h"
#include <string.h>

// Static token for demo purposes
#define EXPECTED_TOKEN "Bearer secret123"

bool auth_check_token(const char *header_value, size_t len) {
    if (len != strlen(EXPECTED_TOKEN)) return false;
    return memcmp(header_value, EXPECTED_TOKEN, len) == 0;
}

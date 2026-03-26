#ifndef ANET_H
#define ANET_H
#include <fcntl.h>
static inline int anetCloexec(int fd) {
    int flags;
    if ((flags = fcntl(fd, F_GETFD)) == -1) return -1;
    if (fcntl(fd, F_SETFD, flags | FD_CLOEXEC) == -1) return -1;
    return 0;
}
#endif

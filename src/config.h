#ifndef CONFIG_H
#define CONFIG_H

#if defined(__APPLE__) || defined(__FreeBSD__)
#define HAVE_KQUEUE 1
#elif defined(__linux__)
#define HAVE_EPOLL 1
#endif

#include <sys/time.h>
#include <stdint.h>
#include <pthread.h> // ae.c expects pthread_mutex_t sometimes



#endif

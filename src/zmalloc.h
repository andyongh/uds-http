#ifndef ZMALLOC_H
#define ZMALLOC_H
#include <stdlib.h>
#define zmalloc malloc
#define zrealloc realloc
#define zcalloc calloc
#define zfree free
#endif

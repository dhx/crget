#ifndef _BUFFER_H
#define _BUFFER_H

#include <sys/types.h>

typedef struct buffer {
        uint8_t *bptr, *dptr;
        size_t bsize, dsize;
} *buffer_t;

buffer_t buffer_create();
void buffer_destroy(buffer_t b);

ssize_t buffer_prepend(buffer_t b, void *buffer, size_t nbytes);
ssize_t buffer_add(buffer_t b, void *buffer, size_t nbytes);
ssize_t buffer_get(buffer_t b, void *buffer, size_t nbytes);
size_t buffer_size(buffer_t b);
void buffer_flush(buffer_t b);

#endif

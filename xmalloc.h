#ifndef XMALLOC_H
#define XMALLOC_H

#include <sys/types.h>

#ifndef NULL
#define NULL 0
#endif

#define ALLOC(type)           (type##_t)xmalloc(sizeof(struct type))
#define ALLOCA(type, size)    (type##_t)xmalloc(sizeof(struct type) * size)

void *xmalloc(size_t size);
void *xcalloc(size_t count, size_t size);
void *xrealloc(void *ptr, size_t size);
char *xstrdup(const char *str);
void xfree(void *ptr);

#endif

#include <sys/types.h>
#include <inttypes.h>
#include <stdlib.h>
#include <string.h>

#include "buffer.h"
#include "xmalloc.h"

buffer_t buffer_create()
{
	buffer_t b;

	b = ALLOC(buffer);
	b->bptr = b->dptr = NULL;
	b->bsize = b->dsize = 0;

	return b;
}

void buffer_destroy(buffer_t b)
{
	buffer_flush(b);
	xfree(b);
}

ssize_t buffer_prepend(buffer_t b, void *buffer, size_t nbytes)
{
	uint8_t *t;

	t = xmalloc(nbytes + b->dsize);
	memcpy(t, buffer, nbytes);
	memcpy(t + nbytes, b->dptr, b->dsize);
	xfree(b->bptr);

	b->bptr = b->dptr = t;
	b->dsize += nbytes;
	b->bsize = b->dsize;

	return nbytes;
}

ssize_t buffer_add(buffer_t b, void *buffer, size_t nbytes)
{
        if(nbytes < 1)
                return 0;

        if(b->bptr == NULL) {
		b->bptr = (uint8_t *)xmalloc(nbytes);
		
                b->dptr = b->bptr;
                b->bsize = b->dsize = nbytes;

                memcpy(b->bptr, buffer, nbytes);

                return nbytes;
        }

        if(b->bsize != b->dsize && (b->bsize - b->dsize - (b->dptr - b->bptr)) < nbytes) 
		b->bptr = (uint8_t *)xrealloc(b->bptr, b->dsize + nbytes);
                
        memmove(b->bptr, b->dptr, b->dsize);
        memcpy(b->bptr + b->dsize, buffer, nbytes);
        b->dsize += nbytes;
        b->dptr = b->bptr;

        return nbytes;
}

ssize_t buffer_get(buffer_t b, void *buffer, size_t nbytes)
{
        int ret;
        
        if(b->dsize == 0)
                return 0;

        if(nbytes >= b->dsize) {
                memcpy(buffer, b->dptr, b->dsize);
		ret = b->dsize;
                xfree(b->bptr);
                b->bptr = b->dptr = NULL;
                b->bsize = b->dsize = 0;

                return ret;
        }

        memcpy(buffer, b->dptr, nbytes);
        b->dptr += nbytes;
        b->dsize -= nbytes;

        return nbytes;
}

size_t buffer_size(buffer_t b)
{
	return b->dsize;
}

void buffer_flush(buffer_t b)
{
	if(b->bptr != NULL) {
		xfree(b->bptr);
		b->bptr = NULL;
	}

	b->dptr = NULL;
	b->bsize = b->dsize = 0;
}

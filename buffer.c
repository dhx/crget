/*
   buffer.c - Generic buffering code
   Copyright (C)2002-03 Anthony Arcieri
   All rights reserved.

   Redistribution and use in source and binary forms, with or without
   modification, are permitted provided that the following conditions
   are met:

 * Redistributions of source code must retain the above copyright
 notice, this list of conditions and the following disclaimer.

 * Redistributions in binary form must reproduce the above copyright
 notice, this list of conditions and the following disclaimer in the
 documentation and/or other materials provided with the distribution.

 * The name of Anthony Arcieri may not be used to endorse or promote 
 products derived from this software without specific prior written 
 permission.

 THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
 "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
 LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR
 A PARTICULAR PURPOSE ARE DISCLAIMED.  IN NO EVENT SHALL THE COPYRIGHT 
 OWNER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, 
 SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED 
 TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR
 PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF
 LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING
 NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF THIS
 SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

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

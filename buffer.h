/*
   buffer.h - Generic buffering code
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

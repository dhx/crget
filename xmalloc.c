/*
   xmalloc.c - Malloc wrappers
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
#include <stdlib.h>
#include <string.h>
#include <stdio.h>

#include "xmalloc.h"

void *xmalloc(size_t size)
{
	void *ret;

	if((ret = malloc(size)) == NULL) {
		perror("malloc");

		exit(-1);
	}

	return ret;
}

void *xcalloc(size_t count, size_t size)
{
	void *ret;

	if((ret = calloc(count, size)) == NULL) {
		perror("calloc");

		exit(-1);
	}

	return ret;
}

void *xrealloc(void *ptr, size_t size)
{
	void *ret;

	if((ret = realloc(ptr, size)) == NULL) {
		perror("realloc");

		exit(-1);
	}

	return ret;
}

char *xstrdup(const char *str)
{
	char *ret;

	if((ret = strdup(str)) == NULL) {
		perror("strdup");

		exit(-1);
	}

	return ret;
}

void xfree(void *ptr)
{
	free(ptr);
}

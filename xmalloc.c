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

#include <stdio.h>
#include <stdarg.h>
#include <stdlib.h>

#include "output.h"

static int quiet = 0;

void print(const char *format, ...)
{
	va_list ap;

	if(quiet)
		return;

	va_start(ap, format);
	vfprintf(stderr, format, ap);
	fflush(stderr);
	va_end(ap);
}

void fatal(const char *format, ...)
{
	va_list ap;

	va_start(ap, format);
	vfprintf(stderr, format, ap);
	va_end(ap);

	exit(EXIT_FAILURE);
}

void draw_bar(int cur, int max)
{
	int p = (cur * 100) / max, b = p / 5, i;

	if(cur != max) 
		fprintf(stderr, "\r %d%%\t[", p);
	else
		fprintf(stderr, "\r100%%\t[");

	for(i = 0; i < 20; i++) {
		if(b >= i)
			fputc('*', stderr);
		else
			fputc(' ', stderr);
	}

	fprintf(stderr, "]\t%d / %d ", cur, max);

	fflush(stderr);
}

void set_quiet()
{
	quiet = 1;
}

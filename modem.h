/*
   modem.h - Code for accessing and dialing modems
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

#ifndef MODEM_H
#define MODEM_H

#include <sys/types.h>
#include <termios.h>

typedef struct modem {
	int fd;
	struct termios tio;
} *modem_t;

modem_t modem_init(char *device);
void modem_close(modem_t m);
void modem_destroy(modem_t m);

int modem_flush(modem_t m);
ssize_t modem_read(modem_t m, void *buf, size_t nbytes, int timeout);
int modem_reset(modem_t m);
ssize_t modem_command(modem_t m, char *instr, char *outstr, int len, int timeout);
ssize_t modem_command(modem_t m, char *instr, char *outstr, int len, int timeout);
int modem_dial(modem_t m, char *number);

#endif

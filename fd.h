/*
   fd.h - File descriptor wrapper providing abstract serial or TCP functionality
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

#ifndef FD_H
#define FD_H

#include <sys/types.h>
#include <termios.h>

#include "modem.h"
#include "buffer.h"

typedef struct fd {
	int fd, type;
	buffer_t b;
	struct termios tio;
} *fd_t;

fd_t fd_init_serial(char *device);
fd_t fd_init_modem(modem_t m);
fd_t fd_init_rawfd(int fd);
void fd_destroy(fd_t s);

ssize_t fd_read_raw(fd_t s, void *buffer, size_t nbytes, unsigned int seconds);
int fd_read(fd_t s, void *buffer, size_t nbytes, unsigned int seconds);
ssize_t fd_read_line(fd_t s, char *buffer, size_t nbytes, unsigned int seconds);
ssize_t fd_write(fd_t s, const void *buffer, size_t nbytes);
int fd_flush(fd_t s);
ssize_t fd_buffer_count(fd_t s);
ssize_t fd_buffer_count_tm(fd_t s, unsigned int seconds);

#endif

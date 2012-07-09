/*
   fd.c - File descriptor wrapper providing abstract serial or TCP functionality
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
#include <sys/time.h>
#include <sys/ioctl.h>
#include <inttypes.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <fcntl.h>
#include <unistd.h>
#include <termios.h>
#include <errno.h>

#include "buffer.h"
#include "modem.h"
#include "fd.h"
#include "xmalloc.h"
#include "output.h"

fd_t fd_init_serial(char *device)
{
	fd_t f = ALLOC(fd);
	struct termios newtio;

	f->type = 1;

	if((f->fd = open(device, O_RDWR | O_NOCTTY)) < 0) {
		perror(device);

		if(errno == ENOENT) {
			fatal("Error #301: Datalogger device does not exist\n");
			exit(EXIT_FAILURE);
		}

		xfree(f);
		return NULL;
	}

	if(tcgetattr(f->fd, &f->tio) < 0) {
		xfree(f);
		return NULL;
	}

	if(tcgetattr(f->fd, &newtio) < 0) {
		xfree(f);
		return NULL;
	}

	newtio.c_cflag |= CLOCAL | CREAD;
	newtio.c_iflag &= ~(IXON|IXOFF|IXANY);
	newtio.c_iflag &= ~(INLCR|IGNCR|ICRNL);
	newtio.c_lflag &= ~(ICANON|ECHO|ECHOE|ISIG);

	if(tcsetattr(f->fd, TCSANOW, &newtio) < 0) {
		xfree(f);
		return NULL;
	}

	f->b = buffer_create();

	return f;
}

fd_t fd_init_modem(modem_t m)
{
	fd_t f;
	struct termios newtio;

	if(tcsetattr(m->fd, TCSANOW, &m->tio) < 0) 
		return NULL;

	if(fcntl(m->fd, F_SETFL, O_NDELAY) < 0) 
		return NULL;

	if(tcdrain(m->fd) < 0)  
		return NULL;

	if(tcflush(m->fd, TCIOFLUSH) < 0) 
		return NULL;

	f = ALLOC(fd);
	f->fd = m->fd;
	f->type = 1;

	if(tcgetattr(f->fd, &f->tio) < 0) {
		xfree(f);
		return NULL;
	}

	if(tcgetattr(f->fd, &newtio) < 0) {
		xfree(f);
		return NULL;
	}

	newtio.c_cflag |= CLOCAL | CREAD;
	newtio.c_iflag &= ~(IXON|IXOFF|IXANY);
	newtio.c_iflag &= ~(INLCR|IGNCR|ICRNL); 
	newtio.c_lflag &= ~(ICANON|ECHO|ECHOE|ISIG);

	if(tcsetattr(f->fd, TCSANOW, &newtio) < 0) {
		xfree(f);
		return NULL;
	}

	f->b = buffer_create();

	return f;
}

fd_t fd_init_rawfd(int descriptor)
{
	fd_t f;

	f = ALLOC(fd);
	f->fd = descriptor;
	f->type = 0;
	f->b = buffer_create();

	return f;
}

void fd_destroy(fd_t f)
{
	buffer_destroy(f->b);
	if(f->type) 
		tcsetattr(f->fd, TCSANOW, &f->tio);
	close(f->fd);
	// DHX valgrind says that the f is never freed... so we free it here
	xfree(f);
}

ssize_t fd_read_raw(fd_t f, void *buffer, size_t nbytes, unsigned int seconds)
{
	ssize_t ret, c = 0, t;
	int8_t *bptr = (int8_t *)buffer;

	fd_set fds;
	struct timeval tv;

	if((c = buffer_size(f->b)) > 0) {
		if(c >= nbytes)
			return buffer_get(f->b, buffer, nbytes);

		buffer_get(f->b, bptr, c);
		nbytes -= c;
		bptr += c;
	}

	tv.tv_sec = seconds;
	tv.tv_usec = 0;

	FD_ZERO(&fds);
	FD_SET(f->fd, &fds);

	if((t = select(f->fd + 1, &fds, NULL, NULL, &tv)) < 1) {
		if(c > 0)
			return c;

		return -1;
	}

	if((ret = read(f->fd, bptr, nbytes)) < 0) {
		if(c > 0)
			return c;

		return -1;
	}

	return c + ret;
}

int fd_read(fd_t f, void *buffer, size_t nbytes, unsigned int seconds)
{
	int c;

	while(nbytes > 0) {
		if((c = fd_read_raw(f, buffer, nbytes, seconds)) < 0)
			return -1;

		buffer += c;
		nbytes -= c;
	}

	return 0;
}

ssize_t fd_write(fd_t f, const void *buffer, size_t nbytes)
{
	return write(f->fd, buffer, nbytes);
}

int fd_flush(fd_t f)
{
	return tcflush(f->fd, TCIOFLUSH);
}

ssize_t fd_read_line(fd_t f, char *buffer, size_t nbytes, unsigned int seconds)
{
	int c, n;
	char *bptr, *eptr;

	bptr = buffer;
	n = nbytes - 1;

	memset(buffer, '\0', nbytes);

	do {
		if((c = fd_read_raw(f, bptr, n, seconds)) < 0) 
			return -1;

		if((eptr = strpbrk(bptr, "\r\n")) != NULL) {
			if(eptr[1] == '\n')
				buffer_add(f->b, eptr + 2, c - (eptr - bptr + 2));
			else
				buffer_add(f->b, eptr + 1, c - (eptr - bptr + 1));

			*eptr = '\0';

			return 0;
		}

		bptr += c;
		n -= c;
	} while(n > 0);

#ifdef DEBUG
	debug("Warning: Buffer full during serial read");
#endif

	return 0;
}

ssize_t fd_buffer_count(fd_t f)
{
	ssize_t ret;

	if(ioctl(f->fd, FIONREAD, &ret) < 0)
		return -1;

	return buffer_size(f->b) + ret;

}

ssize_t fd_buffer_count_tm(fd_t s, unsigned int seconds)
{
	fd_set f;
	struct timeval tv;

	tv.tv_sec = seconds;
	tv.tv_usec = 0;

	FD_ZERO(&f);
	FD_SET(s->fd, &f);

	if(select(s->fd + 1, &f, NULL, NULL, &tv) < 0) 
		return -1; 

	if(!FD_ISSET(s->fd, &f)) { 
#ifdef DEBUG
		debug("Warning: Timeout during serial read"); 
#endif
		return -1; 
	}

	return fd_buffer_count(s);
}

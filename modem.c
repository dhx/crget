/*
   modem.c - Code for accessing and dialing modems
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
#include <sys/ioctl.h>
#include <sys/time.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <fcntl.h>
#include <termios.h>

#include "modem.h"
#include "output.h"
#include "xmalloc.h"

/* Baud rate to use (datalogger supports max of 9600) */
#define BAUDRATE B19200

/* Number of times to send ATZ to modem before giving up */
#define INIT_RETRIES    10

/* Number of seconds to wait after dialing */
#define DIAL_TIMEOUT    120

/* This initializes the modem and returns a handle to the structure */
modem_t modem_init(char *device)
{
	modem_t m = ALLOC(modem);
	struct termios newtio;

	if((m->fd = open(device, O_RDWR | O_NOCTTY)) < 0) {
		perror(device);
		xfree(m);
		return NULL;
	}

	if(tcdrain(m->fd) < 0) {
		perror(device);
		close(m->fd);
		xfree(m);
		return NULL;
	}

	if(tcgetattr(m->fd, &m->tio) < 0) {
		perror(device);
		close(m->fd);
		xfree(m);
		return NULL;
	}

	memset(&newtio, 0, sizeof(newtio));
	cfsetospeed(&newtio, (speed_t)BAUDRATE);
	cfsetispeed(&newtio, (speed_t)BAUDRATE);
	newtio.c_cflag = CRTSCTS | CS8 | CLOCAL | CREAD; /* | BAUDRATE */
	newtio.c_iflag = IGNPAR | ICRNL;
	newtio.c_oflag = 0;
	newtio.c_lflag = ICANON;
	newtio.c_cc[VINTR]    = 0;     /* Ctrl-c */ 
	newtio.c_cc[VQUIT]    = 0;     /* Ctrl-\ */
	newtio.c_cc[VERASE]   = 0;     /* del */
	newtio.c_cc[VKILL]    = 0;     /* @ */
	newtio.c_cc[VEOF]     = 4;     /* Ctrl-d */
	newtio.c_cc[VTIME]    = 0;     /* inter-character timer unused */
	newtio.c_cc[VMIN]     = 1;     /* blocking read until 1 character arrive
					  s */
	newtio.c_cc[VSTART]   = 0;     /* Ctrl-q */ 
	newtio.c_cc[VSTOP]    = 0;     /* Ctrl-s */
	newtio.c_cc[VSUSP]    = 0;     /* Ctrl-z */
	newtio.c_cc[VEOL]     = 0;     /* '\0' */
	newtio.c_cc[VREPRINT] = 0;     /* Ctrl-r */
	newtio.c_cc[VDISCARD] = 0;     /* Ctrl-u */
	newtio.c_cc[VWERASE]  = 0;     /* Ctrl-w */
	newtio.c_cc[VLNEXT]   = 0;     /* Ctrl-v */
	newtio.c_cc[VEOL2]    = 0;     /* '\0' */

	if(tcflush(m->fd, TCIFLUSH) < 0) {
		perror(device);
		close(m->fd);
		xfree(m);
		return NULL;
	}

	if(tcsetattr(m->fd, TCSANOW, &newtio) < 0) {
		perror(device);
		close(m->fd);
		xfree(m);
		return NULL;
	}

	return m;
}

/* Close a modem's file descriptor in preparation for destroying it */
void modem_close(modem_t m)
{
	close(m->fd);
}

/* Free the resources associated with a modem type */
void modem_destroy(modem_t m)
{
	xfree(m);
}

int modem_flush(modem_t m)
{
	int i;
	char buf[16];

	if(ioctl(m->fd, FIONREAD, &i) < 0) {
		perror("ioctl");
		return -1;
	}

	while(i != 0) {
		if(read(m->fd, buf, i < 16 ? i : 16) < 0) {
			perror("read");
			return -1;
		}

		if(ioctl(m->fd, FIONREAD, &i) < 0) {
			perror("ioctl");
			return -1;
		}
	}

	return 0;
}

ssize_t modem_read(modem_t m, void *buf, size_t nbytes, int timeout)
{
	fd_set f;
	struct timeval tv;

	tv.tv_sec = timeout;
	tv.tv_usec = 0;

	FD_ZERO(&f);
	FD_SET(m->fd, &f);

	if(select(m->fd + 1, &f, NULL, NULL, &tv) < 0) {
		perror("select");
		return -1;
	}

	if(!FD_ISSET(m->fd, &f)) 
		return -1;

	return read(m->fd, buf, nbytes);
}

int modem_reset(modem_t m)
{
	int i = 0, x = 0;
	char buf[16], c;

	modem_flush(m);

	do {
		write(m->fd, "+++", 3);
		usleep(500000L);

		if(write(m->fd, "ATZ\r\n", 5) < 0) {
			perror("write");
			return -1;
		}

		memset(buf, '\0', 16);

		while(x < 15) {
			if(modem_read(m, &c, 1, 5) < 0) 
				break;

			if(c == '\r')
				continue;

			if(c != '\n') {
				buf[x++] = c;
				continue;
			}

			if(x == 0)
				continue;

			buf[x] = '\0';

			if(!strcmp("ATZ", buf) || !strcmp("+++ATZ", buf)) {
				x = 0;
				memset(buf, '\0', 16);
				continue;
			}

			break;
		}

		if(i++ == INIT_RETRIES) {
			print("initializiation error, retrying... ");
			return -1;
		}
	} while(strcmp(buf, "OK"));

	modem_flush(m);

	if(modem_command(m, "ATL0", buf, 16, 10) < 0)
		return -1;

	if(strcmp(buf, "OK")) { 
		print("Unexpected response setting modem volume: %s\n", buf);
		return -1;
	}

	return 0;
}

ssize_t modem_command(modem_t m, char *instr, char *outstr, int len, int timeout)
{
	int x = 0;
	char c;

	if(write(m->fd, instr, strlen(instr)) < 0) {
		perror("write");
		return -1;
	}

	if(write(m->fd, "\r\n", 2) < 0) {
		perror("write");
		return -1;
	}

	while(x < len) {
		if(modem_read(m, &c, 1, timeout) < 0) {
			perror("read");
			return -1;
		}

		if(c == '\r')
			continue;

		if(c == '\n') {
			if(x == 0)
				continue;

			outstr[x++] = '\0';

			if(!strcmp(instr, outstr)) {
				x = 0;
				continue;
			}

			break;
		}

		outstr[x++] = c;
	}

	if(x == len) 
		outstr[len - 1] = '\0';

	return x;
}

int modem_dial(modem_t m, char *number)
{
	char *cmd, ret[32];

	cmd = (char *)xmalloc(strlen(number) + 5);

	strcpy(cmd, "ATDT");
	strcat(cmd, number);

	if(modem_command(m, cmd, ret, 32, DIAL_TIMEOUT) < 0) {
		free(cmd);
		return -1;
	}

	free(cmd);

	if(strncmp(ret, "CONNECT", 7) != 0) {
		if(strstr(ret, "BUSY") != NULL) {
			puts("The line is busy");
			return 1;
		}

		if(strstr(ret, "DIALTONE") != NULL) {
			puts("No dialtone");
			return 2;
		}

		if(strstr(ret, "CARRIER") != NULL) {
			puts("No carrier");
			return 3;
		}

		print("Error while dialing %s: %s\n", number, ret);
		return -1;
	}

	return 0;
}

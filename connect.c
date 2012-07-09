/*
   connect.c - Wrappers for connecting to dataloggers through various methods
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
#include <sys/socket.h>
#include <sys/param.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <netdb.h>
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <string.h>

#include "connect.h"
#include "fd.h"
#include "modem.h"
#include "output.h"

#define MODEM_INIT_ATTEMPTS	3
#define MODEM_DIAL_ATTEMPTS	1

fd_t connect_serial(void *cd)
{
	return fd_init_serial(((struct serial_cd *)cd)->device);
}

fd_t connect_modem(void *cd)
{
	int i = 0;
	struct modem_cd *mcd = (struct modem_cd *)cd;

	modem_t m;
	fd_t fd;

	print("Opening port %s... ", mcd->device);
	if((m = modem_init(mcd->device)) == NULL) { 
		perror(mcd->device); 
		fatal("Error #101: Couldn't open modem device\n");
		exit(EXIT_FAILURE);
	}

	print("port opened.\n");

	print("Initializing modem... ");
	fflush(stdout);
	while(modem_reset(m) < 0) {
		if(i++ > MODEM_INIT_ATTEMPTS) {
			fatal("Error #102: Couldn't reset modem\n");
			exit(EXIT_FAILURE);
		}

		modem_close(m);
		modem_destroy(m);
		sleep(5);

		if((m = modem_init(mcd->device)) == NULL) {
			perror(mcd->device);
			fatal("Error #103: Couldn't initialize modem\n");
			exit(EXIT_FAILURE);
		}
	}

	print("done.\n");
	i = 0;

	do {
		if(i++ != 0 && i <= MODEM_DIAL_ATTEMPTS)
			sleep(5);

		if(i > MODEM_DIAL_ATTEMPTS) {
			fatal("Error #104: Too many dialing attempts, giving up!\n");
			exit(EXIT_FAILURE);
		}

		print("Dialing %s... ", mcd->number);
		fflush(stdout);
	} while(modem_dial(m, mcd->number) != 0);

	print("connected.\n");

	if((fd = fd_init_modem(m)) == NULL) {
		return NULL;
	}

	modem_destroy(m);

	return fd;
}

fd_t connect_tcpip(void *cd)
{
	int fd;

	struct sockaddr_in sin;
	struct hostent *host;
	struct tcpip_cd *tcd = (struct tcpip_cd *)cd;

	memset(&sin, 0, sizeof(struct sockaddr_in));
	sin.sin_family = AF_INET;
	sin.sin_port = htons(tcd->port);

	if((sin.sin_addr.s_addr = inet_addr(tcd->hostname)) == INADDR_NONE) {
		if((host = gethostbyname(tcd->hostname)) == NULL) {
			fatal("Error #105: Couldn't resolve address %s\n", tcd->hostname);
			exit(EXIT_FAILURE);
		}

		memcpy(&sin.sin_addr, host->h_addr, host->h_length);
	}

	if((fd = socket(AF_INET, SOCK_STREAM, 0)) < 0) {
		print("Warning: System low on resources (couldn't create socket)\n");
		return NULL;
	}

	if(connect(fd, (struct sockaddr *)&sin, sizeof(sin)) < 0) {
		print("Warning: Couldn't connect to %s:%d\n", tcd->hostname, tcd->port);
		return NULL;
	}

	return fd_init_rawfd(fd);
}

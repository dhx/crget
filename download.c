/*
   download.c - Code for downloading data from a datalogger
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
#include <stdio.h>

#include "connect.h"
#include "download.h"
#include "fd.h"
#include "format_data.h"
#include "logger.h"
#include "output.h"
#include "xmalloc.h"

/* MAX_CONNECT_ATTEMPTS defines how many times we call the connect function to 
   attempt to establish a connection before giving up */
#define MAX_CONNECT_ATTEMPTS	5

/* MAX_FAILED_ATTEMPTS defines a global counter for how many failures have
   occured while attempting to download the data.  If the counter goes above
   this value the program gives up */
#define MAX_FAILED_ATTEMPTS	5

/* MAX_RECORD_SIZE defines the maximum number of locations we expect in a single
   record.  Be liberal with this number, too many means you might lose a record
   in the event of a sustained system outage, but too few means you may read a
   corrupted record.  In the future writing an autodetection function for this
   value might be helpful.
 */
#define MAX_RECORD_SIZE		100

/* DOWNLOAD_CHUNK_SIZE defines how many chunks the internal function calls
   read at a time and display.
 */
#define DOWNLOAD_CHUNK_SIZE	4096

static logger_t cn_wrapper(fd_t (*connect)(void *cd), void *cd, char *security_code)
{
	int i = 0;
	fd_t fd;
	logger_t l;

	while((fd = connect(cd)) == NULL) {
		if(i++ > MAX_CONNECT_ATTEMPTS)
			fatal("Error: Too many failed attempts to connect to datalogger... giving up!\n");
	}

	while((l = logger_create(fd)) == NULL) {
		if(i++ > MAX_CONNECT_ATTEMPTS) 
			fatal("Error: Too many failed attempts to communicate with datalogger... giving up!\n");
	}

	if(security_code != NULL)
		logger_set_security_level(l, security_code);

	return l;

}

static int download_data(uint8_t **bptr, logger_t l, int start, int end, int filled, int *downloaded)
{
	int total, loc_to_read, loc_to_start, total_read, wrapped;
	uint8_t *buffer;

	if(start > end) 
		total = filled - start + end - 1;
	else
		total = end - start;

	if(start + *downloaded >= filled)
		wrapped = 1;
	else
		wrapped = 0;

	if(*bptr == NULL) 
		buffer = *bptr = (uint8_t *)xmalloc(total * 2);

	while(*downloaded < total) {
		draw_bar(*downloaded, total);

		if(!wrapped) {
			loc_to_start = start + *downloaded; 

			if(loc_to_start >= filled) {
				wrapped = 1;
				continue;
			}

			loc_to_read = filled - loc_to_start;
		} else {
			loc_to_start = *downloaded - (filled - start) + 1;
			loc_to_read = end - loc_to_start;
		}

		if(loc_to_read > DOWNLOAD_CHUNK_SIZE)
			loc_to_read = DOWNLOAD_CHUNK_SIZE;

		if((total_read = logger_read_data(l, *bptr + *downloaded * 2, loc_to_start, loc_to_read)) < 0)
			return -1;

		*downloaded += total_read;
	}

	draw_bar(*downloaded, total);
	print("\n");

	return 0;
}

static int download(FILE *out, fd_t (*connect)(void *cd), void *cd, char *security_code, int clockupd, int user_start_location)
{
	int start_location, end_location, downloaded_locations = 0;
	int reference_location, filled_locations;
	int skew = 0;
	int failures = 0;
	uint8_t *buffer = NULL;

	logger_t l = NULL;

	while(failures < MAX_FAILED_ATTEMPTS) {
		if(l != NULL)
			logger_destroy(l);

		l = cn_wrapper(connect, cd, security_code);

		if(clockupd) {
			if(logger_update_clock(l, &skew) < 0) {
				failures++;
				logger_destroy(l);
				l = NULL;

				continue;
			}

			clockupd = 0;
		}

		if(logger_get_position(l, &reference_location, &filled_locations) < 0) {
			failures++;
			logger_destroy(l);
			l = NULL;
			continue;
		}
	} 

	if(failures >= MAX_FAILED_ATTEMPTS) 
		fatal("Error: Too many failed attempts to communicate with datalogger... giving up!\n");

	/* Normal operation */
	if(user_start_location >= 0) 
		start_location = user_start_location;
	else
		start_location = reference_location + MAX_RECORD_SIZE;

	end_location = reference_location;

	if(start_location > filled_locations)
		start_location = 1;

	while(logger_record_align(l, &start_location)) {
		if(failures++ >= MAX_FAILED_ATTEMPTS)
			fatal("Error: Too many failed attempts to communicate with datalogger... giving up!\n");

		logger_destroy(l);
		l = cn_wrapper(connect, cd, security_code);
	}

	print("Downloading data between locations %d and %d:\n", start_location, end_location);

	while(download_data(&buffer, l, start_location, end_location, filled_locations, &downloaded_locations) < 0) {
		if(failures++ >= MAX_FAILED_ATTEMPTS)
			fatal("Error: Too many failed attempts to communicate with datalogger... giving up!\n");

		logger_destroy(l);
		l = cn_wrapper(connect, cd, security_code);
	}

	process_data(out, buffer, downloaded_locations * 2);

	return end_location;
}


int download_serial(FILE *out, char *device, char *security_code, int clockupd, int start_location)
{
	struct serial_cd scd;
	scd.device = device;

	return download(out, connect_serial, &scd, security_code, clockupd, start_location);
}

int download_modem(FILE *out, char *number, char *device, char *security_code, int clockupd, int start_location)
{
	struct modem_cd mcd;

	mcd.device = device;
	mcd.number = number;

	return download(out, connect_modem, &mcd, security_code, clockupd, start_location);
}

int download_tcpip(FILE *out, char *hostname, int port, char *security_code, int clockupd, int start_location)
{
	struct tcpip_cd tcd;

	tcd.hostname = hostname;
	tcd.port = port;

	return download(out, connect_tcpip, &tcd, security_code, clockupd, start_location);
}

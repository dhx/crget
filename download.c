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
#include <stdlib.h>

#include "connect.h"
#include "download.h"
#include "fd.h"
#include "format_data.h"
#include "logger.h"
#include "output.h"
#include "xmalloc.h"


/* DHX  -- Reduced all retry attempts to one as we will fall out of the
 * logger timeframe on a retry
 * */

/* MAX_CONNECT_ATTEMPTS defines how many times we call the connect function to 
   attempt to establish a connection before giving up */
#define MAX_CONNECT_ATTEMPTS	1

/* MAX_FAILED_ATTEMPTS defines a global counter for how many failures have
   occured while attempting to download the data.  If the counter goes above
   this value the program gives up */
#define MAX_FAILED_ATTEMPTS	3

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
		if(i++ > MAX_CONNECT_ATTEMPTS) {
			fatal("Error #201: Too many failed attempts to connect to datalogger... giving up!\n");
			return NULL;
			//exit(EXIT_FAILURE);
		}
	}

	while((l = logger_create(fd)) == NULL) {
		if(i++ > MAX_CONNECT_ATTEMPTS) {
			fatal("Error #202: Too many failed attempts to communicate with datalogger... giving up!\n");
			return NULL;
			//exit(EXIT_FAILURE);
		}
	}

	if(security_code != NULL)
		logger_set_security_level(l, security_code);

	return l;

}

static int download_data(uint8_t **bptr, logger_t l, int start, int end, int filled, int *downloaded)
{
	int total, loc_to_read, loc_to_start, total_read, wrapped, show_bar=0;
	uint8_t *buffer;

	show_bar = (int)(getenv("HIDE_DOWNLOADBAR")==NULL);

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
		if(show_bar) draw_bar(*downloaded, total);

		if(!wrapped) {
			loc_to_start = start + *downloaded; 

			if(loc_to_start >= filled) {
				wrapped = 1;
				continue;
			}

			// DHX this does not make sense and i suspect it producing error as
			// we are reading out of bounds.
			//loc_to_read = filled - loc_to_start;

			if(start > end) {
			loc_to_read = filled - loc_to_start;
		} else {
				loc_to_read = end - loc_to_start;
			}

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

	if(show_bar) {
	draw_bar(*downloaded, total);
	print("\n");
	}

	return 0;
}

static int download(FILE *out, fd_t (*connect)(void *cd), void *cd, char *security_code, int clockupd, int user_start_location)
{
	int start_location, end_location, downloaded_locations = 0;
	int reference_location, filled_locations, memory_pointer, locations_per_array;
	int skew = 0;
	int failures = 0;
	uint8_t *buffer = NULL;

	logger_t l = NULL;

	while(failures < MAX_FAILED_ATTEMPTS) {
		if(l != NULL)
			logger_destroy(l);

		l = cn_wrapper(connect, cd, security_code);

		if(l == NULL) {
			failures++;
			continue;
		}

		if(clockupd) {
			if(logger_update_clock(l, &skew) < 0) {
				failures++;
				logger_destroy(l);
				continue;
			}

			clockupd = 0;
		}

		if(logger_get_position(l, &reference_location, &filled_locations, &memory_pointer, &locations_per_array) < 0) {
			failures++;
			logger_destroy(l);
			continue;
		}

		break;
	} 

	if(failures >= MAX_FAILED_ATTEMPTS) {
		fatal("Error #203: Too many failed attempts to communicate with datalogger... giving up!\n");
		return -1;
	}

	/* Normal operation */
	if(user_start_location >= 0) 
		start_location = user_start_location;
	else
		start_location = reference_location + MAX_RECORD_SIZE;

	end_location = reference_location;

	if(start_location > filled_locations)
		start_location = 1;

	while(l == NULL || logger_record_align(l, &start_location)) {
		if(failures++ >= MAX_FAILED_ATTEMPTS) {
			logger_destroy(l);
			fatal("Error #204: Too many failed attempts to communicate with datalogger... giving up!\n");
			return -1;
		}

		logger_destroy(l);
		l = cn_wrapper(connect, cd, security_code);
	}

	print("Downloading data between locations %d and %d:\n", start_location, end_location);

	while(l == NULL || download_data(&buffer, l, start_location, end_location, filled_locations, &downloaded_locations) < 0) {


		if(failures++ >= MAX_FAILED_ATTEMPTS) {

			if(downloaded_locations > 100) { // even though we failed to download all data, we still got some...

				// try to remove incomplete arrays

				downloaded_locations = (downloaded_locations / locations_per_array) * locations_per_array;
				end_location = (start_location + downloaded_locations) % filled_locations;

				print("Saving incomplete download (%d Locations)",downloaded_locations);

				break;

			} else {

				logger_destroy(l);
				xfree(buffer);
				fatal("Error #205: Too many failed attempts to communicate with datalogger... giving up!\n");
				return -1;

			}
		}

		logger_destroy(l);
		l = cn_wrapper(connect, cd, security_code);
	}

    // print("start_loc: %d  end_loc: %d  filled_loc: %d  downl_loc: %d \n", start_location, end_location, filled_locations, downloaded_locations);

	process_data(out, buffer, downloaded_locations * 2);
	logger_destroy(l);
	xfree(buffer);




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
	int retval;
        modem_t m;

	mcd.device = device;
	mcd.number = number;

	retval = download(out, connect_modem, &mcd, security_code, clockupd, start_location);

	if((m = modem_init(device)) == NULL) {
			perror(device);
			fatal("Error #206: Couldn't open modem device to terminate connection\n");
			exit(EXIT_FAILURE);
	}

	modem_hangup(m);
	modem_destroy(m);

	return retval;

}

int download_tcpip(FILE *out, char *hostname, int port, char *security_code, int clockupd, int start_location)
{
	struct tcpip_cd tcd;

	tcd.hostname = hostname;
	tcd.port = port;

	return download(out, connect_tcpip, &tcd, security_code, clockupd, start_location);
}

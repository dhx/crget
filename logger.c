/*
   logger.c - Functions for low-level datalogger protocol
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
#include <ctype.h>
#include <string.h>
#include <stdlib.h>
#include <unistd.h>
#include <time.h>

#include "logger.h"
#include "fd.h"
#include "xmalloc.h"
#include "output.h"

/* RESPONSE_TIMEOUT specifies (in seconds) how long to wait for a response
 * from the datalogger for any given command.  If it doesn't respond in
 * the specified number of seconds, it is considered a fatal error. */
#define RESPONSE_TIMEOUT        10      

/* INIT_RETRIES specifies the number of times to send CRLF to the datalogger
 * to get the initial prompt.  After we have the initial prompt all operations
 * are synchronous and shouldn't be prone to weird timing errors. */
#define INIT_RETRIES            10

/* PROMPT_ATTEMPTS specifies how many times we will loop back trying to
 * get a prompt.  We shouldn't have to do this much as we will know rather
 * quickly if we fail.  IMPORTANT: This should divide evenly into
 * response timeout, and MUST NOT BE ZERO. */
#define PROMPT_ATTEMPTS         5

/* PROMPT_CHARACTERS specifies the number of characters to scan through for a
 * prompt before we give up.  This is provided as a sanity limit so we don't
 * loop forever waiting for a prompt.  While it doesn't need to be large, there
 * isn't a problem with it being so. */
#define PROMPT_CHARACTERS       256

/* RESPONSE_LINES specifies the number of lines in which we expect to receive a 
 * response to an issued command. */
#define RESPONSE_LINES          6

/* CLOCK_THRESHOLD defines how many seconds the clock may be off without 
 * updating it. */
#define CLOCK_THRESHOLD         30

/* STANDARD_DATA_CHUNK_SIZE defines how many locations we will read at a
 * given time.  The bigger this is the faster reading data will go.
 * Author's note: I ended up calling on this.  They recommended reading
 * data in 2048 byte (1024 location) chunks. */
#define STANDARD_DATA_CHUNK_SIZE        1024

/* EXCEPTION_DATA_CHUNK_SIZE defines how many locations we will read at
 * a given time in the event that a checksum fails.  For fastest results, 
 * this should be a multiple of STANDARD_DATA_CHUNK_SIZE */
#define EXCEPTION_DATA_CHUNK_SIZE       64

/* MAX_CHECKSUM_FAILURES defines, while in exception mode, how many times
 * a checksum can fail before we give up. */
#define MAX_CHECKSUM_FAILURES           5

/* Initialize the datalogger and return a new logger object */
logger_t logger_create(fd_t s)
{
	int r = 0;
	logger_t l = ALLOC(logger);

	l->p = s;
	l->security_level = 0;

	fd_flush(l->p); 

	do {
		if(fd_write(l->p, "\r\n", 2) < 0) {
			print("Serial error: Couldn't send to device!\n");
			return NULL;
		}

		usleep(125000L);
	} while(fd_buffer_count(l->p) == 0 && r < INIT_RETRIES);

	if(r == INIT_RETRIES) {
		print("Datalogger error: No response from datalogger!\n");
		return NULL;
	}

	return l;
}

void logger_destroy(logger_t l)
{
	fd_destroy(l->p);
	xfree(l);
}

/* Get an asterisk prompt from the datalogger */
static int logger_get_prompt(logger_t l)
{
	int i, a = 0;
	char c;

	if(fd_buffer_count(l->p) != 0) 
		fd_flush(l->p);

	if(fd_write(l->p, "\r\n", 2) < 0) {
		print("Serial error: Couldn't send to device while getting prompt!\n");
		return -1;
	}

	for(i = 0; i < PROMPT_CHARACTERS; i++) {
		while(fd_read_raw(l->p, &c, 1, RESPONSE_TIMEOUT / PROMPT_ATTEMPTS) < 0)
		{
			if(a > PROMPT_ATTEMPTS) {
				print("Serial error: Couldn't read data from buffer while getting prompt! (Possible timeout)\n");
				return -1;
			}

			if(fd_write(l->p, "\r\n", 2) < 0) {
				print("Serial error: Couldn't send to device while getting prompt!\n");
				return -1;
			}

			a++;
		}

		if(c == '*') 
			return 0;
	}

	print("Datalogger error: No response while trying to get prompt!\n");

	return -1;
}

/* Send a command to the datalogger 

   XXX This whole function really needs to be rewritten.  One way to start is to
   remove the get prompt function, and have this function get a prompt before
   it returns, then set a value in logger saying the datalogger is ready for the 
   next command. */
static ssize_t logger_command(logger_t l, char *instr, char *outstr, int len)
{
	int i, c, ec = 0;
	char *outptr;

	memset(outstr, '\0', len);

	if(fd_flush(l->p) < 0) {
		print("Serial error: Unable to flush buffer while sending command!\n");
		return -1;
	}

	if(fd_write(l->p, instr, strlen(instr)) < 0) {
		print("Serial error: Couldn't write to device while sending command!\n");
		return -1;
	}

	if(fd_write(l->p, "\r\n", 2) < 0) {
		print("Serial error: Couldn't write to device while sending command!\n");
		return -1;
	}

	for(i = 0; i < RESPONSE_LINES; i++) {
		if(fd_read_line(l->p, outstr, len, RESPONSE_TIMEOUT) < 0) {
			print("Serial error: Couldn't read from device while sending command!\n");
			return -1;
		}

		if((outptr = strrchr(outstr, '*')) == NULL)
			outptr = outstr;
		else
			outptr++;

		if(strlen(outptr) == 0)
			continue;

		if(c == len)
			continue;

		if(strlen(outptr) == 0)
			continue;

		/* XXX Okay, here's how this goes.  I was running into a 
		   bug where I would get a command back from the
		   datalogger that wasn't the original one I issued.
		   This is a cheap hack to correct this problem.

		   Basically, we wait for the command we issued to be echoed
		   back to us.  If we get something else in the mean time, 
		   the bug has been encountered, so we just reinitialize, retry,
		   and hope for the best.
		 */

		if(!strcmp(instr, outptr)) {
			ec = 1;
			continue;
		}

		if(ec) 
			return strlen(outstr);

		/* At this point we've encountered the bug. */

		if(logger_get_prompt(l) < 0) {
			return -1;
		}

		return logger_command(l, instr, outstr, len);
	}

	print("Datalogger error: Invalid response received while sending command\n");

	return -1;
}

/*
   Each time we set the security level we also verify the checksum
   The first operation should be to set the security level, in order that
   we can perform operations and also as a further check of verifying that
   the datalogger is operating properly.

   This function also returns the following:

   -1:    Indicates an error occured (fd error or checksum mismatch)
0:    Indicates the security code was accepted and we are at a new level
1:    No new security level was returned.  The datalogger may be unlocked.
 */
int logger_set_security_level(logger_t l, char *password)
{
	int i = 0, j = 0, smark = 0, lmark = 0, cmark = 0, tc = 0;
	char *cmd, sl_buf[4], cs_buf[6], c;
	uint16_t checksum = 0;
	int16_t cf = -1;

	if(logger_get_prompt(l) < 0)
		return -1;

	cmd = (char *)xmalloc(strlen(password) + 4);

	strcpy(cmd, password);
	strcat(cmd, "L\r\n\n");

	if(fd_write(l->p, cmd, strlen(cmd)) < 0) {
		xfree(cmd);
		print("Lost communication with datalogger (Serial write failed)\n");
		return -1;
	}

	xfree(cmd);

	do {
		if(tc++ > PROMPT_CHARACTERS) {
			print("Lost communication with datalogger (Didn't receive prompt)\n");
			return -1;
		}

		fd_read_raw(l->p, &c, 1, RESPONSE_TIMEOUT);
		if(!cmark && c != '*') 
			checksum = (checksum + c) % 8192;

		if(cmark == 1 && j < 6)
			cs_buf[j++] = c;
		else if(smark == 1 && i < 4)
			sl_buf[i++] = c;

		if(!isdigit(c)) {
			if(cmark == 1) {
				cmark = 0;
				cs_buf[j < 6 ? j : 5] = '\0';
			} else if(smark == 1) {
				smark = 0;
				sl_buf[i < 4 ? i : 3] = '\0';
			}
		}

		if(lmark == 1) {
			if(c == 'C' && cmark == 0) {
				cf = checksum;
				cmark = 1;
			} else if(c == 'S' && smark == 0) 
				smark = 1;
		} else if(c == '\n')
			lmark = 1;
	} while(!lmark || c != '*');

	if(!j || cf == -1) {
		print("Lost communication with datalogger (No checksum issued)\n");
		return -1;
	} else {
		if(cmark == 1)
			cs_buf[j < 6 ? j : 5] = '\0';

		if(atoi(cs_buf) != cf) {
			print("Error communicating with datalogger (Checksum mismatch)\n");
			return -1;
		}
	}

	if(!i) {
		print("Warning: Failed to set security level (Invalid passcode or datalogger unlocked)\n", password);
		return 1;
	} else {
		if(smark == 1)
			sl_buf[i < 4 ? i : 3] = '\0';

		l->security_level = atoi(sl_buf);
		print("Security level set to %d\n", l->security_level);
	}

	return 0;
}

/*
   This function sets the datalogger's clock to the current date and calculates
   the timeskew between the datalogger's old clock setting and the real one.

IMPORTANT: Because this uses the system clock, it is **ABSOLUTELY ESSENTIAL**
that the current system clock have the right time.  This should be 
accomplished by calling ntpdate on machine startup and running ntpd
immediately afterwards.

Also note that dataloggers are always synced to standard time.

One other important thing to note is that in tm_yday as returned by
localtime(), days are counted starting at zero, whereas in the datalogger
they are counted from one.
 */
int logger_update_clock(logger_t l, int *skew)
{
	time_t t;
	struct tm *tm;
	int i = 0;
	int logger_day = 0, logger_hour = 0, logger_minute = 0, logger_second = 0;
	int real_day, real_hour, real_minute, real_second;
	int real_ysec, logger_ysec;
	int real_skew;

	char outbuf[128], inbuf[128], *bptr, *dptr, *tptr = NULL;

	if(logger_get_prompt(l) < 0)
		return -1;

	if(logger_command(l, "C", inbuf, 128) < 0)
		return -1;

	bptr = inbuf;
	while((dptr = strsep(&bptr, " ")) != NULL) {
		switch(dptr[0]) {
			case 'D':
				logger_day = atoi(dptr + 1);
				break;
			case 'T':
				tptr = dptr + 1;
				break;
		}
	}

	if(tptr != NULL) {
		while((dptr = strsep(&tptr, ":")) != NULL) {
			switch(i) {
				case 0:
					logger_hour = atoi(dptr);
					break;
				case 1:
					logger_minute = atoi(dptr);
					break;
				case 2:
					logger_second = atoi(dptr);
					break;
			}

			i++;
		}
	}

	logger_ysec = (logger_day - 1) * 86400 + logger_hour * 3600 + logger_minute * 60 + logger_second;

	t = time(NULL);
	tm = localtime(&t);

	if(tm->tm_isdst) {
		real_hour = tm->tm_hour - 1;
		if(real_hour < 0) {
			real_hour = 23;
			real_day = tm->tm_yday - 1;
		} else
			real_day = tm->tm_yday;

		real_minute = tm->tm_min;
		real_second = tm->tm_sec;
	} else {
		real_day = tm->tm_yday;
		real_hour = tm->tm_hour;
		real_minute = tm->tm_min;
		real_second = tm->tm_sec;
	}

	real_ysec = real_day * 86400 + real_hour * 3600 + real_minute * 60 + real_second;

	real_skew = real_ysec - logger_ysec;

	if(abs(real_skew) > CLOCK_THRESHOLD) {
		print("Updating clock: Skew of %d seconds is greater than %d second threshold.\n", abs(real_skew), CLOCK_THRESHOLD);
		snprintf(outbuf, 128, "%03d:%02d:%02d:%02dC", real_day + 1, real_hour, real_minute, real_second);

		if(logger_command(l, outbuf, inbuf, 128) < 0)
			return 1;
	} else 
		print("Not updating clock: Skew of %d seconds is within %d second threshold.\n", abs(real_skew), CLOCK_THRESHOLD);

	if(skew != NULL)
		*skew = real_skew;

	return 0;
}

/* Returns the current location along with how many records have been filled */
int logger_get_position(logger_t l, int *reference_location, int *filled_locations)
{
	char status_line[128];
	char *s = status_line, *p, *t;

	if(logger_get_prompt(l) < 0)
		return -1;

	if(logger_command(l, "A", s, 128) < 0)
		return -1;

	if(reference_location != NULL)
		*reference_location = -1;

	if(filled_locations != NULL)
		*filled_locations = -1;

	while((p = strsep(&s, " ")) != NULL) {
		switch(p[0]) {
			case 'R':
				if(reference_location == NULL)
					continue;

				if((t = strchr(p, '+')) == NULL)
					continue;
				t++;

				*reference_location = atoi(t);
				break;
			case 'F':
				if(filled_locations == NULL)
					continue;

				if((t = strchr(p, '+')) == NULL)
					continue;
				t++;

				*filled_locations = atoi(t);
				break;
		}
	}

	if(reference_location != NULL && *reference_location == -1)
		return -1;

	if(filled_locations != NULL && *filled_locations == -1)
		return -1;

	return 0;
}

/* Sets the reference location to the given position */
int logger_set_position(logger_t l, int position)
{
	char cmdbuf[16], buf[128], *p, *bptr;

	if(logger_get_prompt(l) < 0) {
		print("Lost communication with datalogger (Error getting prompt)\n");
		return -1;
	}

	sprintf(cmdbuf, "%dG", position);
	if(logger_command(l, cmdbuf, buf, 128) < 0) {
		print("Error: Lost communication with datalogger (Couldn't send command)\n");
		return -1;
	}

	bptr = buf;
	while((p = strsep(&bptr, " ")) != NULL) {
		if(p[0] == 'L' && p[1] != '\0' && p[2] != '\0') {
			p += 2;
			if((bptr = strchr(p, '.')) != NULL) 
				*bptr = '\0';

			if((unsigned)atoi(p) != position) {
				print("Error while setting position: Returned position is different from specified!\n");
				return -1;
			}

			return 0;
		}
	}

	print("Error while setting position: Protocol error!\n");
	return -1;
}

int logger_record_align(logger_t l, int *location)
{
	char new_status[128], *lp, *tp;

	if(logger_set_position(l, *location) < 0)
		return -1;

	if(logger_get_prompt(l) < 0)
		return -1;

	if(logger_command(l, "B", new_status, 128) < 0)
		return -1;

	if((lp = strstr(new_status, "L+")) == NULL)
		return -1;

	lp++;
	if((tp = strchr(lp, ' ')) == NULL)
		return -1;

	*tp = '\0';
	*location = atoi(lp);

	return 0;
}

/* Add a byte to a checksum being computed.  Internal function */
static void logger_checksum_add_byte(uint8_t s[2], uint8_t byte)
{
	uint8_t t1, t2; 

	t1 = s[1];
	s[1] = s[0];
	t2 = (s[0] << 1) | ((s[0] & 0x80) >> 7);
	s[0] = t2 + t1 + byte; 
}

/*
   This function reads the given number of locations from the current 
   reference location.  It returns the following:

0: success
-1: read error/response error
-2: bad checksum

buffer should be large enough (i.e. 2 * locations bytes) to contain the
requested amount of data, otherwise a buffer overflow will occur.
 */
static ssize_t logger_read_raw_data(logger_t l, uint8_t *buffer, unsigned int locations)
{
	int i = 0;
	uint8_t buf[16], c, s[2];
	uint16_t logger_checksum, our_checksum;

	if(logger_get_prompt(l) < 0)
		return -1;

	snprintf((char *)buf, 16, "%dF\r", locations);

	if(fd_write(l->p, buf, strlen((char *)buf)) < 0)
		return -1;

	do {
		if(fd_read(l->p, &c, 1, RESPONSE_TIMEOUT) < 0)
			return -1;
	} while(c != 'F' && i++ < PROMPT_CHARACTERS);

	if(c != 'F') {
		print("Error: Invalid response from datalogger during download\n");
		return -1;
	}

	/* Skip the CRLF */
	if(fd_read(l->p, buf, 2, RESPONSE_TIMEOUT) < 0)
		return -1;

	/* At this point we should begin receiving binary data */
	if(fd_read(l->p, buffer, 2 * locations, RESPONSE_TIMEOUT) < 0)
		return -1;

	/* Read their checksum */
	if(fd_read(l->p, buf, 2, RESPONSE_TIMEOUT) < 0)
		return -1;

	logger_checksum = buf[0] | (buf[1] << 8);

	s[0] = 0xAA;
	s[1] = 0xAA;

	for(i = 0; i < 2 * locations; i++)
		logger_checksum_add_byte(s, buffer[i]);

	our_checksum = s[1] | (s[0] << 8);

	if(logger_checksum != our_checksum) {
		print("Warning: Checksum mismatch!\n");
		return -2;
	}

	return 0;
}

/* This is called when a checksum occurs in read_raw_data_range */
static int logger_read_data_exception(logger_t l, uint8_t *buffer, unsigned int start_location, unsigned int locations_to_read)
{
	unsigned int read_locations, i;
	int retval;

	while(locations_to_read > 0) {
		read_locations = locations_to_read < EXCEPTION_DATA_CHUNK_SIZE ? locations_to_read : EXCEPTION_DATA_CHUNK_SIZE;

		if(logger_set_position(l, start_location) < 0)
			return -1;

		i = 0;
		do {
			if((retval = logger_read_raw_data(l, buffer, read_locations)) == -1)
				return -1;
		} while(retval == -2 && i++ < MAX_CHECKSUM_FAILURES);

		if(retval == -2)
			return -1;

		buffer += read_locations * 2;
		start_location += read_locations;
		locations_to_read -= read_locations;
	}

	return 0;
}

/* Read data from the datalogger, correcting checksum errors as they appear */
ssize_t logger_read_data(logger_t l, uint8_t *buffer, unsigned int start_location, unsigned int locations_to_read)
{
	unsigned int read_locations;
	size_t locations_in_buffer = 0;

	if(logger_set_position(l, start_location) < 0) {
		print("Error communicating with datalogger (Error setting position)\n");
		return -1;
	}

	while(locations_to_read > 0) {
		read_locations = locations_to_read < STANDARD_DATA_CHUNK_SIZE ? locations_to_read : STANDARD_DATA_CHUNK_SIZE;

		if(logger_read_raw_data(l, buffer, read_locations) < 0) {
			if(logger_set_position(l, start_location) < 0) {
				print("Error communicating with datalogger (Error setting position)\n");
				return -1;
			}

			if(logger_read_data_exception(l, buffer, start_location, read_locations) < 0) {
				print("Error communicating with datalogger (Error reading data)\n");
				return -1;
			}
		}

		buffer += read_locations * 2;
		start_location += read_locations;
		locations_in_buffer += read_locations;
		locations_to_read -= read_locations;
	}

	return locations_in_buffer;
}

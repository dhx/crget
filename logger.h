/*
   logger.h - Functions for low-level datalogger protocol
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

#ifndef LOGGER_H
#define LOGGER_H

#include "fd.h"

/* MAX_RECORD_SIZE defines what we assume to be the maximum number of locations
   contained within a single record.  It is only used with the logger's "backup"
   command, to advance the current location backwards this many locations,
   after which it will advance to the beginning of the first record it
   encounters after the current position.

   XXX If there are problems with missing data, this value being incorrect may
   be a likely cause.  In the future it might be helpful to eliminate the need
   for a fixed arbitrary guess at this value by writing some intelligent code
   to decide where to begin reading.
 */
#define MAX_RECORD_SIZE         100


typedef struct logger {
	fd_t p;
	int security_level;
} *logger_t;

logger_t logger_create(fd_t s);
void logger_destroy(logger_t l);

int logger_set_security_level(logger_t l, char *password);
void logger_calculate_real_time(time_t t, int* real_day, int* real_hour, int* real_minute, int* real_second, int* real_ysec);
int logger_update_clock(logger_t l, int *skew);
int logger_get_position(logger_t l, int *reference_location, int *filled_locations, int *memory_pointer, int *locations_per_array);
int logger_set_position(logger_t l, int position);
int logger_record_align(logger_t l, int *location);
ssize_t logger_read_data(logger_t l, uint8_t *buffer, unsigned int start_location, unsigned int locations_to_read);

#endif

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
int logger_update_clock(logger_t l, int *skew);
int logger_get_position(logger_t l, int *reference_location, int *filled_locations);
int logger_set_position(logger_t l, int position);
int logger_record_align(logger_t l, int *location);
ssize_t logger_read_data(logger_t l, uint8_t *buffer, unsigned int start_location, unsigned int locations_to_read);

#endif

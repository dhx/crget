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
   corrupted record.  In the future it may be a good idea to autodetect the
   beginning of valid data in a datalogger when we have no valid signature.
 */
#define MAX_RECORD_SIZE		100

/* DOWNLOAD_CHUNK_SIZE defines how many chunks the internal function calls
   read at a time and display.
*/
#define DOWNLOAD_CHUNK_SIZE	4096

int get_position_signature(logger_t l, uint32_t *signature, int reference_location, int filled_locations)
{
	signature[0] = reference_location;

	/* Note the first location index is 1, not 0 */
	if(logger_read_data(l, (uint8_t *)&signature[1], 1, 2) < 0) 
		return -1;

	/* XXX This is a bit of a hack, but hopefully this shouldn't happen */
	if(filled_locations < 2)
		filled_locations = 2;

	if(logger_read_data(l, (uint8_t *)&signature[2], filled_locations - 2, 2) < 0)
		return -1;

	return 0;
}

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

static void download(FILE *out, fd_t (*connect)(void *cd), void *cd, char *security_code, int clockupd, char *signature, int ps)
{
	int start_location, end_location, downloaded_locations = 0;
	int reference_location, filled_locations;
	int skew = 0;
	int f = 0, gl = 0;
	uint32_t new_signature[3];
	uint8_t *buffer = NULL;

	logger_t l = NULL;

	do {
		if(l != NULL) {
			logger_destroy(l);
			if(f++ >= MAX_FAILED_ATTEMPTS)
				fatal("Error: Too many failed attempts to communicate with datalogger... giving up!\n");
		}

		l = cn_wrapper(connect, cd, security_code);

		if(clockupd) {
			if(logger_update_clock(l, &skew) < 0) {
				if(f++ < MAX_FAILED_ATTEMPTS) {
					logger_destroy(l);
					l = NULL;

					continue;
				}

				fatal("Error: Too many failed attempts to communicate with datalogger... giving up!\n");
			}

			clockupd = 0;
		}

		if(!gl) {
			if(logger_get_position(l, &reference_location, &filled_locations) < 0) {
				if(f++ < MAX_FAILED_ATTEMPTS) {
					logger_destroy(l);
					l = NULL;

					continue;
				}

				fatal("Error: Too many failed attempts to communicate with datalogger... giving up!\n");
			}

			gl = 1;
		}
	} while(get_position_signature(l, new_signature, reference_location, filled_locations));

	/* Normal operation */
	if(signature != NULL && (signature[1] == new_signature[1] || signature[2] == new_signature[2])) {
		start_location = signature[0];
		end_location = reference_location;
	} else {
		start_location = reference_location + MAX_RECORD_SIZE;
		end_location = reference_location;
	}

	if(start_location > filled_locations)
		start_location = 1;

	while(logger_record_align(l, &start_location)) {
		if(f++ >= MAX_FAILED_ATTEMPTS)
			fatal("Error: Too many failed attempts to communicate with datalogger... giving up!\n");

		logger_destroy(l);
		l = cn_wrapper(connect, cd, security_code);
	}

	print("Downloading data between locations %d and %d:\n", start_location, end_location);

	while(download_data(&buffer, l, start_location, end_location, filled_locations, &downloaded_locations) < 0) {
		if(f++ >= MAX_FAILED_ATTEMPTS)
			fatal("Error: Too many failed attempts to communicate with datalogger... giving up!\n");

		logger_destroy(l);
		l = cn_wrapper(connect, cd, security_code);
	}

	process_data(out, buffer, downloaded_locations * 2);
	if(ps)
		fprintf(out, "\n!s,%08x%08x%08x\n", new_signature[0], new_signature[1], new_signature[2]);
}


void download_serial(FILE *out, char *device, char *security_code, int clockupd, char *signature, int ps)
{
	struct serial_cd scd;
	scd.device = device;

	download(out, connect_serial, &scd, security_code, clockupd, signature, ps);
}

void download_modem(FILE *out, char *number, char *device, char *security_code, int clockupd, char *signature, int ps)
{
	struct modem_cd mcd;

	mcd.device = device;
	mcd.number = number;

	download(out, connect_modem, &mcd, security_code, clockupd, signature, ps);
}

void download_tcpip(FILE *out, char *hostname, int port, char *security_code, int clockupd, char *signature, int ps)
{
	struct tcpip_cd tcd;

	tcd.hostname = hostname;
	tcd.port = port;

	download(out, connect_tcpip, &tcd, security_code, clockupd, signature, ps);
}

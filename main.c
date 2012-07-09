/*
   main.c - Main program
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

#define _GNU_SOURCE
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <ctype.h>
#include <time.h>
#include <string.h>
#include <errno.h>
#include <limits.h>


#include "download.h"
#include "output.h"

#define VERSION	"crget 0.1b, a Campbell Datalogger access utility."
#define DEVICE	"/dev/logger"
#define PORT	2030

int check_arg_type(char *logger)
{
	int i, l = strlen(logger);

	for(i = 0; i < l; i++) {
		if(!isdigit(logger[i])) {
			switch(logger[i]) {
				case '-':
					continue;
				case ',':
					return 1;
				default:
					return 2;
			}
		}
	}

	return 1;
}

void usage()
{
	print("Usage: crget [options] [IP address or phone number]\n\n");

	print("Flags:\n");
	print("  -d <device>\tCommunicate using the given serial device\n");
	print("  -p <port>\tConnect to datalogger using the given TCP/IP port\n");
	print("  -l <location>\tLocation to begin reading from. Can also be a filename.\n");
	print("  -c <code>\tUse the given security code\n");
	print("  -o <file>\tOutput to the given file (- for stdout)\n");
	print("  -C\t\tDon't update datalogger's clock\n");
	print("  -i\t\tForce interpretation of datalogger location as Internet address\n");
	print("  -q\t\tQuiet operation (disables all messages)\n");
	print("  -h\t\tDisplay this help\n");
	print("\n");
	print("Environment Variables:\n");
	print("   MODEM_INITSTRING :\tWhen defined this string will be used to initialize\n");
	print("                     \tthe modem.\n");
	exit(-1);
}

void usage_full()
{
	print("%s\n", VERSION);
	usage();
}

int main(int argc, char **argv)
{
	int r, end_location;

	/* Settings */
	int mode = -1;	/* 0: Local serial  1: Modem  2: TCP/IP */
	int clockupd = -1;
	int port = PORT;
	int startloc = -1;
	long lval = -1;
	char *device = DEVICE;
	char *security_code = NULL;
	char *outfile = NULL;
	char *locfile = NULL;
	char *endptr = NULL;
	char *logger = NULL;
	time_t c;
	struct tm *tm;

	FILE *output_file;
	FILE *location_file;

	while((r = getopt(argc, argv, "d:p:l:c:o:s:Ciqh")) != -1) {
		switch(r) {
			case 'd':
				if(mode != -1) {
					print("Error: Options -d and -p/-i are mutually exclusive\n");
					usage();
				}

				mode = 0;
				device = optarg;
				break;
			case 'p':
				if(mode != -1 && mode != 2) {
					print("Error: Options -d and -p are mutually exclusive\n");
					usage();
				}

				mode = 2;
				if((port = atoi(optarg)) == 0) {
					print("Error: Invalid port specified: %s\n", optarg);
					usage();
				}

				break;
			case 'l':

				locfile = strdup(optarg);
				location_file = fopen(locfile, "r");
				if(location_file != NULL) {
					fscanf(location_file,"%d",&startloc);

					if (getenv("VERBOSE_OUTPUT")!=NULL)
						print("Reading position out of '%s': %d\n",locfile,startloc);

					fclose(location_file);
				} else {

					free(locfile);
					locfile = NULL;

					errno = 0;    /* To distinguish success/failure after call */
					lval = strtol(optarg, &endptr, 0);

					if ((errno == ERANGE && (lval == LONG_MAX || lval == LONG_MIN)) || (errno != 0 && lval == 0)) {
			               perror("No valid position entered");
			               exit(EXIT_FAILURE);
					}

					if (endptr == optarg) {
					   fprintf(stderr, "No position entered (-l).\n");
					   exit(EXIT_FAILURE);
					}

			        if (lval > INT_MAX)
			        {
			        	print("Position %ld too large!\n", lval);
						exit(EXIT_FAILURE);
			        }
			        else if (lval < INT_MIN)
			        {
			            print("Position %ld too small!\n", lval);
						exit(EXIT_FAILURE);
			        }

			        startloc = (int) lval;

				}
				break;
			case 'c':
				if(clockupd != 0)
					clockupd = 1;

				security_code = optarg;
				break;
			case 'o':
				outfile = strdup(optarg);
				break;
			case 'C':
				clockupd = 0;
				break;
			case 'i':
				if(mode != -1 && mode != 2) {
					print("Error: Options -d and -i are mutually exclusive\n");
					usage();
				}

				mode = 2;
				break;
			case 'q':
				set_quiet();
				break;
			case 'h':
				usage_full();
		}
	}
	argc -= optind;
	argv += optind;

	if(argc > 0) {
		if(mode == -1)
			mode = check_arg_type(argv[0]);
		else if(mode == 0)
			mode = 1;

		logger = argv[0];
	} else if(mode == -1)
		mode = 0;
	else if(mode == 2)
		usage_full();

	time(&c);
	tm = localtime(&c);
	print("--%02d:%02d:%02d--  ", tm->tm_hour, tm->tm_min, tm->tm_sec);
	switch(mode) {
		case 0:
			print("Getting data from serial device at %s\n", device);
			break;
		case 1:
			print("Getting data via modem %s from logger at %s\n", device, logger);
			break;
		case 2:
			print("Getting data via TCP/IP connection to %s:%d\n", logger, port);
			break;
	}

	if(outfile != NULL) {
		if(strlen(outfile) == 1 && *outfile == '-') {
			print("           => (standard output)\n");
			output_file = stdout;
		} else {
			print("           => '%s'\n", outfile);
			output_file = fopen(outfile, "a");
			if(output_file == NULL) {
				perror("Could not open filename");
				exit(EXIT_FAILURE);
			}
		}
	} else {
		switch(mode) {
			case 0:
				asprintf(&outfile, "logger_data-%04d%02d%02d", tm->tm_year + 1900, tm->tm_mon + 1, tm->tm_mday);
				break;
			case 1:
			case 2:
				asprintf(&outfile, "logger_data-%s-%04d%02d%02d", logger, tm->tm_year + 1900, tm->tm_mon + 1, tm->tm_mday);
				break;
		}

		print("           => '%s'\n", outfile);
		output_file = fopen(outfile, "a");
		if(output_file == NULL) {
			perror("Could not open filename");
			exit(EXIT_FAILURE);
		}
	}

	switch(mode) {
		case 0:
			end_location = download_serial(output_file, device, security_code, clockupd, startloc);
			break;
		case 1:
			end_location = download_modem(output_file, logger, device, security_code, clockupd, startloc);
			break;
		case 2:
			end_location = download_tcpip(output_file, logger, port, security_code, clockupd, startloc);
			break;
	}

	int exit_code;

	if(end_location == -1) {

	time(&c);
	tm = localtime(&c); 
		print("--%02d:%02d:%02d--  Data download failed\n", tm->tm_hour, tm->tm_min, tm->tm_sec);

		exit_code = EXIT_FAILURE;

	} else {

		time(&c);
		tm = localtime(&c);
		print("--%02d:%02d:%02d--  Data download successful ", tm->tm_hour, tm->tm_min, tm->tm_sec);

	if(strlen(outfile) == 1 && *outfile == '-') 
		print("=> (standard output)\n");
	else 
		print("=> '%s'\n", outfile);


		if(locfile != NULL) { // locfile is set when we have read the position from a file

			if (getenv("VERBOSE_OUTPUT")!=NULL)
				print("Writing back the end location to '%s'.\n",locfile);

			location_file = fopen(locfile,"w");

			// and so we write it back to the file from where we got it
			fprintf(location_file,"%d",end_location);

			fclose(location_file);
			free(locfile);
		}

		exit_code = EXIT_SUCCESS;

	}

	if(output_file != stdout)
		fclose(output_file);

	if(outfile != NULL) {
		free(outfile);
	}

	if (getenv("VERBOSE_OUTPUT")!=NULL)
		print("End location: %d\n",end_location);
	exit(exit_code);
}

#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <ctype.h>
#include <time.h>
#include <string.h>

#include "download.h"
#include "output.h"

#define VERSION	"crget 0.1, a Campbell Datalogger access utility."
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
	print("  -s <sig>\tSignature from previous communication\n");
	print("  -c <code>\tUse the given security code\n");
	print("  -o <file>\tOutput to the given file (- for stdout)\n");
	print("  -C\t\tDon't update datalogger's clock\n");
	print("  -S\t\tAppend the new signature to the end of the data\n");
	print("  -i\t\tForce interpretation of datalogger location as Internet address\n");
	print("  -q\t\tQuiet operation (disables all messages)\n");
	print("  -h\t\tDisplay this help\n");
	exit(EXIT_FAILURE);
}

void usage_full()
{
	print("%s\n", VERSION);
	usage();
}

int main(int argc, char **argv)
{
	int r;

	/* Settings */
	int mode = -1;	/* 0: Local serial  1: Modem  2: TCP/IP */
	int clockupd = -1;
	int port = PORT;
	int ps = 0;
	char *device = DEVICE;
	char *security_code = NULL;
	char *signature = NULL;
	char *outfile = NULL;
	char *logger = NULL;
	time_t c;
	struct tm *tm;

	FILE *output_file;

	while((r = getopt(argc, argv, "d:p:s:c:o:CSiqh")) != -1) {
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
			case 's':
				if(strlen(optarg) != 24) {
					print("Warning: Invalid signature given: %s\n", optarg);
					break;
				}

				signature = optarg;
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
			case 'S':
				ps = 1;
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
	}

	switch(mode) {
		case 0:
			download_serial(output_file, device, security_code, clockupd, signature, ps);
			break;
		case 1:
			download_modem(output_file, logger, device, security_code, clockupd, signature, ps);
			break;
		case 2:
			download_tcpip(output_file, logger, port, security_code, clockupd, signature, ps);
			break;
	}

	time(&c);
	tm = localtime(&c); 
	print("--%02d:%02d:%02d--  Data downloaded successfully ", tm->tm_hour, tm->tm_min, tm->tm_sec);

	if(strlen(outfile) == 1 && *outfile == '-') 
		print("=> (standard output)\n");
	else 
		print("=> '%s'\n", outfile);

	free(outfile);

	if(output_file != stdout)
		fclose(output_file);

	exit(EXIT_SUCCESS);
}

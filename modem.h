#ifndef MODEM_H
#define MODEM_H

#include <sys/types.h>
#include <termios.h>

typedef struct modem {
	int fd;
	struct termios tio;
} *modem_t;

modem_t modem_init(char *device);
void modem_close(modem_t m);
void modem_destroy(modem_t m);

int modem_flush(modem_t m);
ssize_t modem_read(modem_t m, void *buf, size_t nbytes, int timeout);
int modem_reset(modem_t m);
ssize_t modem_command(modem_t m, char *instr, char *outstr, int len, int timeout);
ssize_t modem_command(modem_t m, char *instr, char *outstr, int len, int timeout);
int modem_dial(modem_t m, char *number);
	
#endif

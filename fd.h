#ifndef FD_H
#define FD_H

#include <sys/types.h>
#include <termios.h>

#include "modem.h"
#include "buffer.h"

typedef struct fd {
	int fd, type;
	buffer_t b;
	struct termios tio;
} *fd_t;

fd_t fd_init_serial(char *device);
fd_t fd_init_modem(modem_t m);
fd_t fd_init_rawfd(int fd);
void fd_destroy(fd_t s);

ssize_t fd_read_raw(fd_t s, void *buffer, size_t nbytes, unsigned int seconds);
int fd_read(fd_t s, void *buffer, size_t nbytes, unsigned int seconds);
ssize_t fd_read_line(fd_t s, char *buffer, size_t nbytes, unsigned int seconds);
ssize_t fd_write(fd_t s, const void *buffer, size_t nbytes);
int fd_flush(fd_t s);
ssize_t fd_buffer_count(fd_t s);
ssize_t fd_buffer_count_tm(fd_t s, unsigned int seconds);

#endif

#ifndef CONNECT_H
#define CONNECT_H

#include "fd.h"

struct serial_cd {
	char *device;
};

struct modem_cd {
	char *device;
	char *number;
};

struct tcpip_cd {
	char *hostname;
	int port;
};

fd_t connect_serial(void *cd);
fd_t connect_modem(void *cd);
fd_t connect_tcpip(void *cd);

#endif

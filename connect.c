#include <sys/types.h>
#include <sys/socket.h>
#include <sys/param.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <netdb.h>
#include <stdio.h>
#include <unistd.h>
#include <string.h>

#include "connect.h"
#include "fd.h"
#include "modem.h"
#include "output.h"

#define MODEM_INIT_ATTEMPTS	5
#define MODEM_DIAL_ATTEMPTS	5

fd_t connect_serial(void *cd)
{
	return fd_init_serial(((struct serial_cd *)cd)->device);
}

fd_t connect_modem(void *cd)
{
	int i = 0;
	struct modem_cd *mcd = (struct modem_cd *)cd;

	modem_t m;
	fd_t fd;

	print("Opening port %s... ", mcd->device);
	if((m = modem_init(mcd->device)) == NULL) { 
		perror(mcd->device); 
		fatal("Couldn't open modem device\n");
	}

	print("port opened.\n");

	print("Initializing modem... ");
	fflush(stdout);
	while(modem_reset(m) < 0) {
		if(i++ > MODEM_INIT_ATTEMPTS) 
			fatal("Couldn't reset modem\n");

		modem_close(m);
		modem_destroy(m);
		sleep(5);

		if((m = modem_init(mcd->device)) == NULL) {
			perror(mcd->device);
			fatal("Couldn't initialize modem\n");
		}
	}

	print("done.\n");
	i = 0;

	do {
		if(i++ != 0 && i <= MODEM_DIAL_ATTEMPTS)
			sleep(5);

		if(i > MODEM_DIAL_ATTEMPTS) 
			fatal("Too many dialing attempts, giving up!\n");

		print("Dialing %s... ", mcd->number);
		fflush(stdout);
	} while(modem_dial(m, mcd->number) != 0);

	print("connected.\n");
	
	if((fd = fd_init_modem(m)) == NULL)
		return NULL;

	modem_destroy(m);

	return fd;
}

fd_t connect_tcpip(void *cd)
{
	int fd;
	
	struct sockaddr_in sin;
	struct hostent *host;
	struct tcpip_cd *tcd = (struct tcpip_cd *)cd;

	memset(&sin, 0, sizeof(struct sockaddr_in));
	sin.sin_family = AF_INET;
	sin.sin_port = htons(tcd->port);

	if((sin.sin_addr.s_addr = inet_addr(tcd->hostname)) == INADDR_NONE) {
		if((host = gethostbyname(tcd->hostname)) == NULL) 
			fatal("Error: Couldn't resolve address %s\n", tcd->hostname);

		memcpy(&sin.sin_addr, host->h_addr, host->h_length);
	}

	if((fd = socket(AF_INET, SOCK_STREAM, 0)) < 0) {
		print("Warning: System low on resources (couldn't create socket)\n");
		return NULL;
	}
	
	if(connect(fd, (struct sockaddr *)&sin, sizeof(sin)) < 0) {
		print("Warning: Couldn't connect to %s:%d\n", tcd->hostname, tcd->port);
		return NULL;
	}
	
	return fd_init_rawfd(fd);
}

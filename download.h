#ifndef DOWNLOAD_H
#define DOWNLOAD_H

#include <stdio.h>

int download_serial(FILE *out, char *device, char *security_code, int clockupd, int start_location);
int download_modem(FILE *out, char *number, char *device, char *security_code, int clockupd, int start_location);
int download_tcpip(FILE *out, char *hostname, int port, char *security_code, int clockupd, int start_location);

#endif

#ifndef DOWNLOAD_H
#define DOWNLOAD_H

#include <stdio.h>

void download_serial(FILE *out, char *device, char *security_code, int clockupd, char *signature, int ps);
void download_modem(FILE *out, char *number, char *device, char *security_code, int clockupd, char *signature, int ps);
void download_tcpip(FILE *out, char *hostname, int port, char *security_code, int clockupd, char *signature, int ps);

#endif

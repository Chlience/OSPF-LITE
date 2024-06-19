#ifndef DEBUG_H
#define DEBUG_H

#include <cstdio>
#include <cstdarg>
#include <arpa/inet.h>

int debugf(const char *format, ...);
char* n_ip2string(in_addr_t ip);
char* ip2string(uint32_t ip);

#endif
#include "config.h"

#include <net/if.h>
#include <netinet/ip.h>
#include <bits/stdc++.h>
#include <sys/socket.h>
#include <netinet/ether.h>
#include <netinet/in.h>
#include <stdio.h>
#include <netdb.h>
#include <string.h>
#include <errno.h>
#include <unistd.h>
#include <sys/ioctl.h> 
#include <arpa/inet.h>

uint32_t get_ip_address(const char* nic_name) {
	int socket_fd;
    if ((socket_fd = socket(AF_INET, SOCK_DGRAM, 0)) < 0) {
        perror("SendPacket: socket_fd init");
		return -1;
    }

	struct ifreq ifr;
	memset(&ifr, 0, sizeof(ifr));
	strcpy(ifr.ifr_name, nic_name);

	if (ioctl(socket_fd, SIOCGIFADDR, &ifr) < 0) {
        perror("ioctl() error");
        close(socket_fd);
        return -1;
    }
	
	close(socket_fd);
	return ntohl(((struct sockaddr_in*)&ifr.ifr_addr)->sin_addr.s_addr);
}

uint32_t get_network_mask(const char* nic_name) {
	int socket_fd;
    if ((socket_fd = socket(AF_INET, SOCK_DGRAM, 0)) < 0) {
        perror("SendPacket: socket_fd init");
		return -1;
    }

	struct ifreq ifr;
	memset(&ifr, 0, sizeof(ifr));
	strcpy(ifr.ifr_name, nic_name);
	
	if (ioctl(socket_fd, SIOCGIFNETMASK, &ifr) < 0) {
        perror("ioctl() error");
        close(socket_fd);
        return -1;
    }

	close(socket_fd);
	return ntohl(((struct sockaddr_in*)&ifr.ifr_netmask)->sin_addr.s_addr);
}
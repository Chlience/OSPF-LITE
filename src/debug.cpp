#include "debug.h"

int debugf(const char *format, ...) {
	#ifdef DEBUG
    va_list args;
    va_start(args, format);
    int ret = vprintf(format, args);
    va_end(args);
	return ret;
	#else
	return 0;
	#endif
}


char* n_ip2string(in_addr_t ip) {
    struct in_addr ip_addr;
    ip_addr.s_addr = ip;
    return inet_ntoa(ip_addr);
	// char *inet_ntoa(struct in_addr in);					将网络字节序地址(in_addr) 转换为点分十进制字符串
	// int inet_aton(const char *IP, struct in_addr *addr); 将点分十进制字符串转化为网络字节序地址，并返回对应的整数
}

char* ip2string(uint32_t ip) {
    struct in_addr ip_addr;
    ip_addr.s_addr = htonl(ip);
    return inet_ntoa(ip_addr);
	// char *inet_ntoa(struct in_addr in);					将网络字节序地址(in_addr) 转换为点分十进制字符串
	// int inet_aton(const char *IP, struct in_addr *addr); 将点分十进制字符串转化为网络字节序地址，并返回对应的整数
}
#include "ospf.h"
#include "config.h"
#include <cstdio>
#include <unistd.h>

int main() {
	ospf_init();
	// while(1) {	
	// 	send_ospf_packet(0xc0a8f602, 0x01, "Hello, OSPF!", 12);
	// 	sleep(1);
	// }
	return 0;
}
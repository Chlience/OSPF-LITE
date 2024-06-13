
#include <cstdio>
#include <unistd.h>
#include <pthread.h>

#include "ospf.h"
#include "config.h"
#include "interface.h"

GlobalConfig myconfigs = GlobalConfig();

void config_init() {
	myconfigs.nic_name		= "eth0";	// 手动设定
	myconfigs.ip			= get_ip_address(myconfigs.nic_name);
	myconfigs.network_mask	= get_network_mask(myconfigs.nic_name);
	
	myconfigs.router_id = 0x08080808;	// 手动设定
	myconfigs.area = new OSPFArea();
	myconfigs.area->area_id	= 0;

	OSPFNetwork* network = new OSPFNetwork();
	network->ip		    	= myconfigs.ip;
	network->wildcard_mask 	= ~myconfigs.network_mask;
	myconfigs.area->networks.push_back(network);
}

int main() {
	config_init();

	printf("IP address      = %x\n", myconfigs.ip);
	printf("Network address = %x\n", myconfigs.network_mask);

	ospf_init();
	
	Interface interface;
	interface.ip			= myconfigs.ip;
	interface.network_mask	= myconfigs.network_mask;
	
    pthread_t hello_sender_thread;

	printf("[main]\t\tpthread_create: hello_sender_thread\n");
	pthread_create(&hello_sender_thread, NULL, send_ospf_hello_package_thread, &interface);

    pthread_join(hello_sender_thread, NULL);
	printf("[main]\t\tend\n");
	return 0;
}
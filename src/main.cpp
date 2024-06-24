#include <cstdio>
#include <unistd.h>
#include <pthread.h>

#include "ospf.h"
#include "config.h"
#include "interface.h"
#include "debug.h"

GlobalConfig myconfigs = GlobalConfig();

void config_init() {
	myconfigs.nic_name		= "eth0";	// 手动设定
	myconfigs.ip			= get_ip_address(myconfigs.nic_name);
	myconfigs.ip_interface_mask	= get_network_mask(myconfigs.nic_name);
	
	myconfigs.router_id = 0x08080808;	// 手动设定
	myconfigs.area = new OSPFArea(0);

	OSPFNetwork* network = new OSPFNetwork();
	network->ip		    	= myconfigs.ip;
	network->wildcard_mask 	= ~myconfigs.ip_interface_mask;
	myconfigs.area->networks.push_back(network);
}

void interface_init(Interface* interface) {
	interface->area						= myconfigs.area;
	interface->ip_interface_address		= myconfigs.ip;
	interface->ip_interface_mask		= myconfigs.ip_interface_mask;
}

int main() {
	config_init();
	ospf_init();
	
	Interface interface;
	interface_init(&interface);
	interface.event_interface_up();
	
    pthread_t ospf_hello_sender;
    pthread_t ospf_reciver;

	printf("[main]\t\tpthread_create: ospf_hello_sender\n");
	printf("[main]\t\tpthread_create: ospf_reciver\n");

	pthread_create(&ospf_hello_sender, NULL, send_ospf_hello_packet_thread, &interface);
	pthread_create(&ospf_reciver, NULL, recv_ospf_packet_thread, &interface);

    pthread_join(ospf_hello_sender, NULL);
    pthread_join(ospf_reciver, NULL);

	printf("[main]\t\tend\n");
	return 0;
}
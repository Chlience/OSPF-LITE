#include <cstdio>
#include <unistd.h>
#include <pthread.h>

#include "ospf.h"
#include "config.h"
#include "interface.h"
#include "debug.h"
#include "area.h"

GlobalConfig myconfigs = GlobalConfig();

std::vector<Interface*> interfaces;
std::vector<OSPFArea*> areas;

void config_init() {
	myconfigs.nic_name			= "enp0s20f0u1c2";	// 手动设定
	myconfigs.ip				= get_ip_address(myconfigs.nic_name);
	myconfigs.ip_interface_mask	= get_network_mask(myconfigs.nic_name);
	myconfigs.router_id 		= ntohl(inet_addr("8.8.8.8"));	// 手动设定
}

void interface_init(Interface* interface) {
	interface->ip_interface_address		= myconfigs.ip;
	interface->ip_interface_mask		= myconfigs.ip_interface_mask;
}

int main() {
	config_init();
	ospf_init();

	Interface interface;
	interfaces.push_back(&interface);

	OSPFArea area = OSPFArea(ntohl(inet_addr("0.0.0.0")));
	areas.push_back(&area);
	
	interface_init(&interface);

	OSPFNetwork network = OSPFNetwork(myconfigs.ip, ~myconfigs.ip_interface_mask);
	area.networks.push_back(&network);
	for (auto inter : interfaces) {
		if ((inter->ip_interface_address & inter->ip_interface_mask) == (network.ip & (~network.wildcard_mask))) {
			area.interfaces.push_back(inter);
			inter->area = &area;
		}
	}

	printf("[config info]\n");
	printf("	router id: %s\n", ip2string(myconfigs.router_id));
	printf("\n");

	printf("[interface info]\n");
	printf("    ip addr: %s\n", ip2string(interface.ip_interface_address));
	printf("    network mask: %s\n", ip2string(interface.ip_interface_mask));
	printf("    area id %s\n", ip2string(interface.area->id));

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
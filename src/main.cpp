#include <cstdio>
#include <unistd.h>
#include <pthread.h>

#include "ospf.h"
#include "config.h"
#include "interface.h"
#include "debug.h"
#include "area.h"

GlobalConfig myconfig = GlobalConfig();

std::vector<Interface*> interfaces;
std::vector<OSPFArea*> areas;

void config_init() {
	myconfig.nic_name			= "enp0s20f0u1c2";	// 手动设定
	myconfig.ip				= get_ip_address(myconfig.nic_name);
	myconfig.ip_interface_mask	= get_network_mask(myconfig.nic_name);
	myconfig.router_id 		= ntohl(inet_addr("8.8.8.8"));	// 手动设定
	myconfig.ls_sequence_cnt = INITIAL_SEQUENCE_NUMBER;
}

void interface_init(Interface* interface) {
	interface->ip_interface_address		= myconfig.ip;
	interface->ip_interface_mask		= myconfig.ip_interface_mask;
}

int main() {
	config_init();
	ospf_init();

	Interface loopback;
	loopback.state = InterfaceState::S_LOOPBACK;
	loopback.ip_interface_address = ntohl(inet_addr("192.168.246.200"));
	loopback.ip_interface_mask = ntohl(inet_addr("255.255.255.000"));
	interfaces.push_back(&loopback);

	Interface interface;
	interfaces.push_back(&interface);

	OSPFArea area = OSPFArea(ntohl(inet_addr("0.0.0.0")));
	areas.push_back(&area);
	
	interface_init(&interface);

	OSPFNetwork network = OSPFNetwork(myconfig.ip, ~myconfig.ip_interface_mask);
	area.networks.push_back(&network);
	for (auto inter : interfaces) {
		if ((inter->ip_interface_address & inter->ip_interface_mask) == (network.ip & (~network.wildcard_mask))) {
			area.interfaces.push_back(inter);
			inter->area = &area;
		}
	}

	printf("[config info]\n");
	printf("    router id: %s\n", ip2string(myconfig.router_id));
	for (auto inter : interfaces) {
		printf("[interface info]\n");
		printf("    ip addr: %s\n", ip2string(inter->ip_interface_address));
		printf("    network mask: %s\n", ip2string(inter->ip_interface_mask));
		printf("    area id %s\n", ip2string(inter->area->id));
	}

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
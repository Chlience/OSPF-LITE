#include <stdlib.h>
#include <stdio.h>
#include <unistd.h>

#include "lsa.h"
#include "config.h"
#include "debug.h"
#include "ospf.h"

extern GlobalConfig myconfig;

LSARouter* generate_router_lsa(Interface* interface) {
	LSARouter* router_lsa = new LSARouter;
	router_lsa->header.ls_age	= 1;
	router_lsa->header.options	= myconfig.ospf_options;
	router_lsa->header.ls_type	= 1;
	router_lsa->header.link_state_id = myconfig.router_id;
	router_lsa->header.advertising_router = myconfig.router_id;
	router_lsa->header.ls_seq_num	= myconfig.ls_sequence_cnt;
	++ myconfig.ls_sequence_cnt;
	router_lsa->header.ls_checksum	= 0;
	router_lsa->header.length		= sizeof(LSAHeader);
	router_lsa->zero0 = 0;
	router_lsa->b_v = 0;
	router_lsa->b_e = 0;
	router_lsa->b_b = 0;
	router_lsa->zero1 = 0;
	router_lsa->links_num = 0;
	router_lsa->header.length += sizeof(uint8_t) * 2 + sizeof(uint16_t);

	LSARouterLink link;

	for (auto inter : interface->area->interfaces) {
		if (inter->state == InterfaceState::S_LOOPBACK) {
			link.type 		= 3;
			link.link_id 	= inter->ip_interface_address;
			link.link_data 	= 0xffffffff;
			link.metric		= 0;
		} else {
			bool flag = false;
			if (inter->state != InterfaceState::S_WAITING) {
				for (auto neibor : inter->neighbors) {
					if (neibor->state == NeighborState::S_FULL) {
						if (inter->state == InterfaceState::S_DR || neibor->ip == inter->dr) {
							link.type 		= 2;
							link.link_id 	= inter->dr;
							link.link_data 	= inter->ip_interface_address;
							link.metric		= inter->cost;
							flag = true;
						}
					}
				}
			}
			if (inter->state == InterfaceState::S_WAITING || !flag) {
				link.type		= 3;
				link.link_id	= inter->ip_interface_address & inter->ip_interface_mask;
				link.link_data	= inter->ip_interface_mask;
				link.metric		= inter->cost;
			}
		}
		router_lsa->links.push_back(link);
		router_lsa->links_num++;
		router_lsa->header.length += sizeof(LSARouterLink);
	}

	/* TODO 增加描述主机的 lsa */

	return router_lsa;

}

LSANetwork* generate_network_lsa(Interface* interface) {
	LSANetwork* network_lsa = new LSANetwork;
	network_lsa->header.ls_age	= 1;
	network_lsa->header.options = myconfig.ospf_options;
	network_lsa->header.ls_type = 2;
	network_lsa->header.link_state_id = interface->ip_interface_address;
	network_lsa->header.advertising_router = myconfig.router_id;
	network_lsa->header.ls_seq_num 	= myconfig.ls_sequence_cnt;
	++myconfig.ls_sequence_cnt;
	network_lsa->header.ls_checksum = 0;
	network_lsa->header.length		= sizeof(LSAHeader);
	network_lsa->network_mask		= interface->ip_interface_mask;
	network_lsa->header.length += sizeof(uint32_t);
	
	for (auto neighbor : interface->neighbors) {
		if (neighbor->state == NeighborState::S_FULL) {
			network_lsa->attached_routers.push_back(neighbor->id);
			network_lsa->header.length += sizeof(uint32_t);
		}
	}
	return network_lsa;
}

void update_lsas(Interface* interface) {
	char* data = (char*)malloc(1024);
	OSPFLsu* lsu = (OSPFLsu*)data;
	lsu->num = 0;
	char* ptr = data + sizeof(OSPFLsu);
	LSDB* lsdb = &interface->area->lsdb;
	pthread_mutex_lock(&lsdb->lsa_mutex);

	if (interface->state == InterfaceState::S_DR) {
		LSANetwork* network_lsa = generate_network_lsa(interface);
		LSANetwork* network_lsa_old = lsdb->find_network_lsa(network_lsa->header.link_state_id, network_lsa->header.advertising_router);
		bool is_flooding = false;
		if (network_lsa_old == nullptr) {
			lsdb->network_lsas.push_back(network_lsa);
			is_flooding = true;
		} else if (lsa_header_cmp((LSAHeader*)network_lsa, (LSAHeader*)network_lsa_old) < 0) {
			lsdb->remove_network_lsa(network_lsa->header.link_state_id, network_lsa->header.advertising_router);
			is_flooding = true;
		} else {
			is_flooding = false;
		}
		if (is_flooding) {
			lsu->num = htonl(ntohl(lsu->num) + 1);
			LSAHeader* lsa_header = (LSAHeader*) ptr;
			memcpy(lsa_header, LSAHeader::hton(&network_lsa->header), sizeof(LSAHeader));
			ptr += sizeof(LSAHeader);
			*(uint32_t*)ptr = htonl(network_lsa->network_mask);
			ptr += sizeof(uint32_t);
			for (auto router: network_lsa->attached_routers) {
				*(uint32_t*)ptr = htonl(router);
				ptr += sizeof(uint32_t);
			}
			lsa_header->ls_checksum = htons(lsa_checksum(lsa_header, ntohs(lsa_header->length)));
		}
		delete network_lsa;
	}

	{
		LSARouter* router_lsa = generate_router_lsa(interface);
		LSARouter* router_lsa_old = lsdb->find_router_lsa(router_lsa->header.link_state_id, router_lsa->header.advertising_router);
		bool is_flooding = false;
		if (router_lsa_old == nullptr) {
			lsdb->router_lsas.push_back(router_lsa);
			is_flooding = true;
		} else if (lsa_header_cmp((LSAHeader*)router_lsa, (LSAHeader*)router_lsa_old) < 0) {
			lsdb->remove_router_lsa(router_lsa->header.link_state_id, router_lsa->header.advertising_router);
			is_flooding = true;
		} else {
			is_flooding = false;
		}
		if (is_flooding) {
			lsu->num = htonl(ntohl(lsu->num) + 1);
			LSARouter* router_lsa_net = (LSARouter*)ptr;
			memcpy((LSAHeader*)router_lsa_net, LSAHeader::hton(&router_lsa->header), sizeof(LSAHeader));
			router_lsa_net->zero0 = router_lsa->zero0;
			router_lsa_net->b_v = router_lsa->b_v;
			router_lsa_net->b_e = router_lsa->b_e;
			router_lsa_net->b_b = router_lsa->b_b;
			router_lsa_net->zero1 = router_lsa->zero1;
			router_lsa_net->links_num = htons(router_lsa->links_num);
			ptr += sizeof(LSAHeader) + sizeof(uint8_t) * 2 + sizeof(uint16_t);
			for (auto link : router_lsa->links) {
				LSARouterLink* router_link = (LSARouterLink*)ptr;
				router_link->link_id	= htonl(link.link_id);
				router_link->link_data	= htonl(link.link_data);
				router_link->type		= link.type;
				router_link->tos_num	= link.tos_num;
				router_link->metric		= htons(link.metric);
				if (link.tos_num != 0) {
					perror("TODO: TOS\n");
				}
				ptr += sizeof(LSARouterLink);
			}
			router_lsa_net->header.ls_checksum = htons(lsa_checksum((LSAHeader*)router_lsa_net, ntohs(router_lsa_net->header.length)));
		}
		delete router_lsa;
	}
	pthread_mutex_unlock(&lsdb->lsa_mutex);
	
	if (lsu->num != 0) {
		if (interface->state == InterfaceState::S_DR || interface->state == InterfaceState::S_BACKUP) {
			send_ospf_packet(ntohl(inet_addr("224.0.0.5")), T_LSU, data, ptr - data, interface);
		} else {
			send_ospf_packet(ntohl(inet_addr("224.0.0.6")), T_LSU, data, ptr - data, interface);
		}
	}
	free(data);
}

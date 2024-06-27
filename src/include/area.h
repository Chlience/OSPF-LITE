#ifndef AREA_H
#define AREA_H

#include <cstdint>
#include <stddef.h>
#include <vector>

#include "ospf.h"
#include "link_state.h"

class Interface;

/* TODO 在指定 network 时，将所有满足条件的接口加入到区域中 */
class OSPFNetwork {
	public:
	uint32_t				ip;
	uint32_t				wildcard_mask;

	OSPFNetwork(uint32_t ip, uint32_t wildcard_mask):ip(ip),wildcard_mask(wildcard_mask) {}
};

class OSPFArea {
	public:
	uint32_t					id;
	std::vector<OSPFNetwork*>	networks;
	LSDB						lsdb;
	std::vector<Interface*>		interfaces;

	OSPFArea(uint32_t area_id):id(area_id) {}
	
	int add_network(uint32_t ip, uint32_t wildcard_mask);
};


#endif
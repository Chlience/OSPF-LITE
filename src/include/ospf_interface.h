#ifndef OSPF_INTERFACE_H
#define OSPF_INTERFACE_H

#include <list>
#include <stdint.h>

#include "neighbor.h"

class OSPFNetwork {
	public:
	uint32_t				ip;
	uint32_t				wildcard_mask;
};

class OSPFArea {
	public:
	uint32_t				area_id		= 0;
	std::list<OSPFNetwork*>	networks	= std::list<OSPFNetwork*>();
};

#endif
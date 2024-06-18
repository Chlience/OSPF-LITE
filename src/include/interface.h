#ifndef INTERFACE_H
#define INTERFACE_H

#include <list>
#include <stdint.h>
#include <set>

#include "neighbor.h"

class Interface {
	public:
	uint32_t			ip;
	uint32_t			network_mask;
	uint32_t			dr;
	uint32_t			bdr;
	// std::list<Neighbor*> neighbors;
	std::set<Neighbor> neighbors;
	
	std::set<Neighbor>::iterator find_neighbor(in_addr_t);
	void add_neighbor(Neighbor);
};

#endif
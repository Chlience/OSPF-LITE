#ifndef INTERFACE_H
#define INTERFACE_H

#include <list>
#include <stdint.h>

#include "neighbor.h"

class Interface {
	public:
	uint32_t			ip;
	uint32_t			network_mask;
	uint32_t			dr;
	uint32_t			bdr;
	std::list<Neighbor*> neighbors;
};

#endif
#ifndef INTERFACE_H
#define INTERFACE_H

#include <list>
#include <stdint.h>
#include <set>

#include "neighbor.h"

enum struct NetworkType : uint8_t {
    T_P2P = 1,
    T_BROADCAST,
    T_NBMA,
    T_P2MP,
    T_VIRTUAL,
};

enum struct InterfaceState : uint8_t {
    S_DOWN = 0,
    S_LOOPBACK,
    S_WAITING,
    S_POINT2POINT,
    S_DROTHER,
    S_BACKUP,
    S_DR,
};

class Interface {
	public:
	NetworkType			type 	= NetworkType::T_BROADCAST; 
	uint32_t			ip 	= 0;
	uint32_t			network_mask = 0;
	uint32_t			dr	= 0;
	uint32_t			bdr = 0;
	std::list<Neighbor*> neighbors;
	InterfaceState		state	= InterfaceState::S_DOWN;
	bool 				can_be_dr = true;

	Interface() = default;
	// ~Interface();

	Neighbor* find_neighbor(uint32_t);
	void add_neighbor(Neighbor*);

	void event_interface_up();
	void event_wait_timer();
	void event_backup_seen();
	void event_neighbor_change();
	void event_loop_ind();
	void event_unloop_ind();
	void event_interface_down();
};

#endif
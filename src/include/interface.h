#ifndef INTERFACE_H
#define INTERFACE_H

#include <list>
#include <stdint.h>
#include <set>
#include <atomic>

#include "neighbor.h"
#include "area.h"

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

enum struct ElectionType : uint8_t {
    T_NORMAL = 0,
    T_NODR,
    T_NOBDR,
};

class Interface {
	public:
	NetworkType			type 	= NetworkType::T_BROADCAST; 
	InterfaceState		state	= InterfaceState::S_DOWN;
	uint32_t			ip_interface_address 	= 0;
	uint32_t			ip_interface_mask 		= 0;
	OSPFArea*			area 					= nullptr;
	uint32_t			hello_interval			= 10;
	uint32_t			router_dead_interval	= 40;
	uint32_t			inf_trans_delay			= 1;
	uint32_t			router_priority			= 1;
	// Hello Timer
	// Wait Timer
	std::list<Neighbor*> neighbors;
	uint32_t			dr	= 0;
	uint32_t			bdr = 0;
	// Interface output cost
	uint32_t			rxmt_interval	= 5;
	uint32_t			au_type			= 0;
	
    uint16_t    		mtu = 1500;

	std::atomic<bool>	waiting_timeout	= false;
	pthread_t			waiting_timer;

	uint16_t			cost = 1;

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
	void call_election();
	// void* waiting_timer(void*);
};

#endif
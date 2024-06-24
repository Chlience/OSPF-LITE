#include <pthread.h>
#include "interface.h"
#include "neighbor.h"
#include "debug.h"
#include "ospf.h"

void Neighbor::event_hello_received() {
	printf("Neighbor %s event_hello_received", ip2string(ip));
	if (state == NeighborState::S_DOWN) {
		state = NeighborState::S_INIT;
		printf(" from DOWN to INIT\n");
	} else {
		printf(" reset clock\n");
	}
}

void Neighbor::event_2way_received() {
	printf("Neighbor %s event_2way_received", ip2string(ip));
	if (state == NeighborState::S_INIT) {
		/* 决定是否与邻居建立连接 */
		if (interface->type == NetworkType::T_P2P
		|| interface->type 	== NetworkType::T_P2MP
		|| interface->type 	== NetworkType::T_VIRTUAL
		|| interface->dr 	== interface->ip_interface_address
		|| interface->bdr 	== interface->ip_interface_address
		|| interface->dr 	== ip
		|| interface->bdr	== ip) {
			dd_seq_num = 0;		// 指定一个 dd_seq_num
			is_master = true; 	// 指是自己（不是 neighbor）为 master
			state = NeighborState::S_EXSTART;
			printf(" from INIT to EXSTART\n");
			is_empty_dd_sender_running = true;
			pthread_create(&empty_dd_sender, nullptr, send_empty_dd_packet_thread, (void*)this);
		} else {
			state = NeighborState::S_2WAY;
			printf(" from INIT to 2WAY\n");
		}
	} else {
        printf("\n");
	}
}

void Neighbor::event_1way_received() {
	printf("Neighbor %s event_1way_received", ip2string(ip));
	if (state >= NeighborState::S_2WAY) {
		printf(" from 2WAY or higher state to INIT\n");
		state = NeighborState::S_INIT;
		link_state_retransmission_list.clear();
		database_summary_list.clear();
		link_state_request_list.clear();
	}
	else if (state == NeighborState::S_INIT) {
		printf("\n");
	}
	else {
		printf(" REJCET\n");
	}
}

void Neighbor::event_adj_ok() {
	printf("Neighbor %s event_adj_ok", ip2string(ip));
	if (state == NeighborState::S_2WAY) {
		/* 决定是否与邻居建立连接 */
		if (interface->type == NetworkType::T_P2P
		|| interface->type 	== NetworkType::T_P2MP
		|| interface->type 	== NetworkType::T_VIRTUAL
		|| interface->dr 	== interface->ip_interface_address
		|| interface->bdr 	== interface->ip_interface_address
		|| interface->dr 	== this->ip
		|| interface->bdr	== this->ip) {
			dd_seq_num = 0;		// 指定一个 dd_seq_num
			is_master = true; 	// 指是自己（不是 neighbor）为 master
			state = NeighborState::S_EXSTART;
			if (is_empty_dd_sender_running == false) {
				pthread_create(&empty_dd_sender, nullptr, send_empty_dd_packet_thread, (void*)this);
			} else {
				empty_dd_sender_stop = true;
				pthread_join(empty_dd_sender, nullptr);
				empty_dd_sender_stop = false;
				pthread_create(&empty_dd_sender, nullptr, send_empty_dd_packet_thread, (void*)this);
			}
			printf(" from 2WAY to EXSTART\n");
		} else {
			state = NeighborState::S_2WAY;
			printf(" stay in 2WAY\n");
		}
	} else if (state >= NeighborState::S_EXSTART) {
		if (interface->type == NetworkType::T_P2P
		|| interface->type 	== NetworkType::T_P2MP
		|| interface->type 	== NetworkType::T_VIRTUAL
		|| interface->dr 	== interface->ip_interface_address
		|| interface->bdr 	== interface->ip_interface_address
		|| interface->dr 	== this->ip
		|| interface->bdr	== this->ip) {
			// DO NOTHING
		} else {
			state = NeighborState::S_2WAY;
			database_summary_list.clear();
			link_state_request_list.clear();
			printf(" from EXSTART or higher state to 2WAY\n");
		}
	} else {
        printf(" REJECT\n");
	}
}

void Neighbor::event_negotiation_done() {
	printf("Neighbor %s event_negotiation_done", ip2string(ip));
	if (state == NeighborState::S_EXSTART) {
		LSDB* lsdb = &interface->area->lsdb;
		for (auto router_lsa: lsdb->router_lsas) {
			database_summary_list.push_back(router_lsa->header);
		}
		for (auto network_lsa: lsdb->network_lsas) {
			database_summary_list.push_back(network_lsa->header);
		}
		state = NeighborState::S_EXCHANGE;
		printf(" from EXSTART to EXCHANGE\n");
	} else {
		printf(" REJECT\n");
	}
}

void Neighbor::event_seq_number_mismatch() {
	printf("Neighbor %s event_seq_number_mismatch", ip2string(ip));
	if (state >= NeighborState::S_EXCHANGE) {
		state = NeighborState::S_EXSTART;
		link_state_retransmission_list.clear();
		database_summary_list.clear();
		link_state_request_list.clear();
		if (is_empty_dd_sender_running == false) {
			pthread_create(&empty_dd_sender, nullptr, send_empty_dd_packet_thread, (void*)this);
		} else {
			empty_dd_sender_stop = true;
			pthread_join(empty_dd_sender, nullptr);
			empty_dd_sender_stop = false;
			pthread_create(&empty_dd_sender, nullptr, send_empty_dd_packet_thread, (void*)this);
		}
		printf(" from EXCHANGE or greater to EXSTART\n");
	} else {
		printf(" REJECT\n");
	}
}

void Neighbor::event_exchange_done() {
	printf("Neighbor %s event_exchange_done", ip2string(ip));
	if (state == NeighborState::S_EXCHANGE) {
		if (link_state_request_list.empty()) {
			state = NeighborState::S_FULL;
			printf(" from EXCHANGE to FULL\n");
		}
		else {
			state = NeighborState::S_LOADING;
			/* 发送 LSR 请求新的 LSA */
			printf(" from EXCHANGE to LOADING\n");
		}
	} else {
		printf(" REJECT\n");
	}
}

void Neighbor::event_loading_done() {
	printf("Neighbor %s event_loading_done", ip2string(ip));
	if (state == NeighborState::S_LOADING) {
		state = NeighborState::S_FULL;
		printf(" from LOADING to FULL\n");
	} else {
		printf(" REJECT\n");
	}
}
#include "interface.h"
#include "neighbor.h"
#include "debug.h"

void Neighbor::event_hello_received() {
	printf("Neighbor %s event_hello_received", ip2string(id));
	if (state == NeighborState::S_DOWN) {
		state = NeighborState::S_INIT;
		printf(" from DOWN to INIT\n");
	} else {
		printf(" reset clock\n");
	}
}

void Neighbor::event_2way_received() {
	printf("Neighbor %s event_2way_received", ip2string(id));
	if (state == NeighborState::S_INIT) {
		if (interface->type == NetworkType::T_P2P
		|| interface->type 	== NetworkType::T_P2MP
		|| interface->type 	== NetworkType::T_VIRTUAL
		|| interface->dr 	== interface->ip
		|| interface->bdr 	== interface->ip
		|| interface->dr 	== ip
		|| interface->bdr	== ip) {
			dd_seq_num = 0;		// 指定一个 dd_seq_num
			is_master = true; 	// 指是自己（不是 neighbor）为 master
			state = NeighborState::S_EXSTART;
			// 开始发送空 DD 包
			// pthread_create(&empty_dd_send_thread, &myconfigs::thread_attr, threadSendEmptyDDPackets, (void*)this);
			printf(" from INIT to EXSTART\n");
		} else {
			state = NeighborState::S_2WAY;
			printf(" from INIT to 2WAY\n");
		}
	} else {
		printf("\n");
	}
}

void Neighbor::event_1way_received() {
	printf("Neighbor %s event_1way_received", ip2string(id));
	if (state >= NeighborState::S_2WAY) {
		printf(" from 2WAY or higher state to INIT\n");
		state = NeighborState::S_INIT;
		/* 清除连接状态重传列表、数据库汇总列表和连接状态请求列表中的LSA */
	}
	else if (state == NeighborState::S_INIT){
		printf("\n");
	}
	else {
		printf(" something WRONG with it\n");
	}
}
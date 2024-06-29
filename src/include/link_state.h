#ifndef LINK_STATE_H
#define LINK_STATE_H

#include <vector>
#include <cstdint>

#include "const.h"

/** LSAHeader 中 (ls_type, link_state_id, advertising_router) 唯一标识一条 LSA
 * ls_type				为 lsa 的类型
 * link_state_id		描述 lsa 的网络部件。
 * 						对于 Router LSA，link_state_id 为生成该 LSA 的路由器的 Router ID
 * 						对于 Network LSA，link_state_id 为 DR 的 IP 地址
 * 						对于 Summary LSA(3)，link_state_id 为目标网络的 IP 地址
 * 						对于 Summary LSA(4)，link_state_id 为描述的 ASBR 路由器的 Router ID
 * 						对于 AS External LSA，link_state_id 为网络的 IP 地址
 * advertising_router	为生成该 LSA 的路由器 Router ID
 * */
struct LSAHeader {
	uint16_t    ls_age;
	uint8_t     options;
	uint8_t     ls_type;
	uint32_t    link_state_id;
	uint32_t    advertising_router;
	uint32_t    ls_seq_num;
	uint16_t    ls_checksum;
	uint16_t    length;

	static LSAHeader* ntoh(const LSAHeader*);
	static LSAHeader* hton(const LSAHeader*);
}; // 20 bytes

int lsa_header_cmp(const LSAHeader*, const LSAHeader*);
uint16_t lsa_checksum(const LSAHeader*);
uint16_t lsa_checksum_ru(const void* data, size_t len, int checksum_offset);

enum LSAType : uint8_t {
	LSA_ROUTER = 1,
	LSA_NETWORK,
	LSA_SUMNET,
	LSA_SUMASB,
	LSA_ASEXTERNAL,
};

struct LSARouterLink {
	uint32_t	link_id;
	uint32_t	link_data;
	uint8_t		type;
	uint8_t		tos_num;
	uint16_t	metric;
	/* TOSs */
	/* 没考虑 tos_num 不等于 0 的情况 */
}; // 12 bytes

struct LSARouter {
	LSAHeader	header;
	uint8_t		zero0 	: 5;
	uint8_t		b_v		: 1;
	uint8_t		b_e		: 1;
	uint8_t		b_b		: 1;
	uint8_t		zero1;
	uint16_t	links_num;
	std::vector<LSARouterLink> links;
};

struct LSANetwork {
	LSAHeader	header;
	uint32_t	network_mask;
	std::vector<uint32_t> attached_routers;
};

class LSDB {
	public:
	std::vector<LSARouter*> router_lsas;
	std::vector<LSANetwork*> network_lsas;
	LSARouter* find_router_lsa(uint32_t link_state_id, uint32_t advertising_router);
	LSANetwork* find_network_lsa(uint32_t link_state_id, uint32_t advertising_router);
	void install_router_lsa(void *data);
	void install_network_lsa(void *data);
	void remove_router_lsa(uint32_t link_state_id, uint32_t advertising_router);
	void remove_network_lsa(uint32_t link_state_id, uint32_t advertising_router);
};

#endif
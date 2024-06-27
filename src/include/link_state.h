#ifndef LINK_STATE_H
#define LINK_STATE_H

#include <vector>
#include <cstdint>

#include "const.h"

struct LSAHeader {
	uint16_t    ls_age;
	uint8_t     options;
	uint8_t     ls_type;
	uint32_t    link_state_id;
	uint32_t    advertising_router;
	uint32_t    ls_seq_num;
	uint16_t    ls_checksum;
	uint16_t    length;

	static LSAHeader* ntoh(const LSAHeader* netHeader);
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
};

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
};

class LSDB {
	public:
	std::vector<LSARouter*> router_lsas;
	std::vector<LSANetwork*> network_lsas;
	LSARouter* find_router_lsa(uint32_t link_state_id, uint32_t advertising_router);
	LSANetwork* find_network_lsa(uint32_t link_state_id, uint32_t advertising_router);
	void install_lsa_router(void *data);
};


#endif
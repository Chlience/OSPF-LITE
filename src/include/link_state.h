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
}; // 20 bytes

int lsa_header_cmp(const LSAHeader*, const LSAHeader*);

enum LSAType : uint8_t {
	LSA_ROUTER = 1,
	LSA_NETWORK,
	LSA_SUMNET,
	LSA_SUMASB,
	LSA_ASEXTERNAL,
};

struct LSARouter {
	LSAHeader	header;
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
};


#endif
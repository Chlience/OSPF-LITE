#include "link_state.h"
#include "math.h"

int lsa_header_cmp(const LSAHeader* a, const LSAHeader* b) {
	if (a->ls_seq_num != b->ls_seq_num) {
		return a->ls_seq_num < b->ls_seq_num ? 1 : -1;
	} else {
		if (a->ls_checksum != b->ls_checksum) {
			return a->ls_checksum < b->ls_checksum ? 1 : -1;
		} else {
			if (a->ls_age == MAX_AGE && b->ls_age == MAX_AGE) {
				return 0;
			}
			else if (a->ls_age == MAX_AGE) {
				return -1;
			}
			else if (b->ls_age == MAX_AGE) {
				return 1;
			}
			else {
				if (abs(a->ls_age - b->ls_age) >= MAX_AGE_DIFF) {
					return a->ls_age > b->ls_age ? 1 : -1;
				}
				else {
					return 0;
				}
			}
		}
	}
}

LSARouter* LSDB::find_router_lsa(uint32_t link_state_id, uint32_t advertising_router) {
	for (auto& lsa : router_lsas) {
		if (lsa->header.link_state_id == link_state_id && lsa->header.advertising_router == advertising_router) {
			return lsa;
		}
	}
	return nullptr;
}

LSANetwork* LSDB::find_network_lsa(uint32_t link_state_id, uint32_t advertising_router) {
	for (auto& lsa : network_lsas) {
		if (lsa->header.link_state_id == link_state_id && lsa->header.advertising_router == advertising_router) {
			return lsa;
		}
	}
	return nullptr;
}
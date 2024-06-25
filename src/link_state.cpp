#include <arpa/inet.h>

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


uint16_t calculate_fletcher_checksum(const uint8_t* data, size_t len) {
	uint16_t sum1 = 0, sum2 = 0;
	for (size_t i = 0; i < len; i++) {
		sum1 = (sum1 + data[i]) % 0xff;
		sum2 = (sum2 + sum1) % 0xff;
	}
	return (sum2 << 8) | sum1;
}

/**
 * 注意为网络序
 */
// uint16_t lsa_checksum(const LSAHeader* lsa_header) {
// 	LSAHeader lsa_header_copy = *lsa_header;
// 	lsa_header_copy.ls_checksum = 0;
// 	uint16_t length = ntohs(lsa_header_copy.length) - 2;
// 	lsa_header_copy.length = htons(length);
// 	uint8_t* p = ((uint8_t*)&lsa_header_copy) + 2;
// 	return htons(calculate_fletcher_checksum(p, length));
// }

// uint16_t lsa_checksum(const LSAHeader* lsa_header) { // from stackoverflow
// 	LSAHeader lsa_header_copy = *lsa_header;
// 	LSAHeader* lsa = &lsa_header_copy;

// 	unsigned char* data  = (unsigned char*) lsa;
// 	unsigned short bytes = ntohs(lsa->length);
// 	unsigned short sum1  = 0xff, sum2 = 0xff;

// 	/* RFC : The Fletcher checksum of the complete contents of the LSA,
// 	*       including the LSA header but excluding the LS age field.
// 	*/
// 	data += 2; bytes -= 2;

// 	lsa->ls_checksum = 0;
// 	while (bytes) {
// 		size_t len = bytes > 20 ? 20 : bytes;
// 		bytes -= len;
// 		do {
// 			sum2 += sum1 += *data++;
// 		} while (--len);
// 		sum1 = (sum1 & 0xff) + (sum1 >> 8);
// 		sum2 = (sum2 & 0xff) + (sum2 >> 8);
// 	}
// 	sum1 = (sum1 & 0xff) + (sum1 >> 8);
// 	sum2 = (sum2 & 0xff) + (sum2 >> 8);
// 	return sum2 << 8 | sum1;
// }


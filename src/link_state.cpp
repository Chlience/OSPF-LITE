#include <arpa/inet.h>
#include <cstring>
#include <algorithm>
#include <pthread.h>
#include <unistd.h>

#include "link_state.h"
#include "math.h"
#include "debug.h"
#include "interface.h"
#include "config.h"

extern GlobalConfig myconfig;

LSAHeader* LSAHeader::ntoh(const LSAHeader* netHeader) {
	static LSAHeader hostHeader;
	hostHeader.ls_age = ntohl(netHeader->ls_age);
	hostHeader.options = netHeader->options;
	hostHeader.ls_type = netHeader->ls_type;
	hostHeader.link_state_id = ntohl(netHeader->link_state_id);
	hostHeader.advertising_router = ntohl(netHeader->advertising_router);
	hostHeader.ls_seq_num = ntohl(netHeader->ls_seq_num);
	hostHeader.ls_checksum = ntohs(netHeader->ls_checksum);
	hostHeader.length = ntohs(netHeader->length);
	return &hostHeader;
}

LSAHeader* LSAHeader::hton(const LSAHeader* header_host) {
	static LSAHeader header_net;
	header_net.ls_age = htonl(header_host->ls_age);
	header_net.options = header_host->options;
	header_net.ls_type = header_host->ls_type;
	header_net.link_state_id = htonl(header_host->link_state_id);
	header_net.advertising_router = htonl(header_host->advertising_router);
	header_net.ls_seq_num = htonl(header_host->ls_seq_num);
	header_net.ls_checksum = htons(header_host->ls_checksum);
	header_net.length = htons(header_host->length);
	return &header_net;
}

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
	auto it = std::find_if(router_lsas.begin(), router_lsas.end(), [link_state_id, advertising_router](LSARouter* lsa) {
		return lsa->header.link_state_id == link_state_id && lsa->header.advertising_router == advertising_router; });
	if (it != router_lsas.end()) {
		return *it;
	} else {
		return nullptr;
	}
}

LSANetwork* LSDB::find_network_lsa(uint32_t link_state_id, uint32_t advertising_router) {
	auto it = std::find_if(network_lsas.begin(), network_lsas.end(), [link_state_id, advertising_router](LSANetwork* lsa) {
		return lsa->header.link_state_id == link_state_id && lsa->header.advertising_router == advertising_router; });
	if (it != network_lsas.end()) {
		return *it;
	} else {
		return nullptr;
	}
}


void LSDB::remove_router_lsa(uint32_t link_state_id, uint32_t advertising_router) {
	router_lsas.erase(std::remove_if(router_lsas.begin(), router_lsas.end(), [link_state_id, advertising_router](LSARouter* lsa) {
		return lsa->header.link_state_id == link_state_id && lsa->header.advertising_router == advertising_router; }));
}

void LSDB::remove_network_lsa(uint32_t link_state_id, uint32_t advertising_router) {
	network_lsas.erase(std::remove_if(network_lsas.begin(), network_lsas.end(), [link_state_id, advertising_router](LSANetwork* lsa) {
		return lsa->header.link_state_id == link_state_id && lsa->header.advertising_router == advertising_router; }));
}

void LSDB::install_router_lsa(void* data) {
	LSAHeader* lsa_header_host = new LSAHeader(*LSAHeader::ntoh((LSAHeader*)data));
	/* 删除旧 LSA */
	auto it = std::find_if(router_lsas.begin(), router_lsas.end(), [lsa_header_host](LSARouter* lsa) {
		return lsa->header.link_state_id == lsa_header_host->link_state_id && lsa->header.advertising_router == lsa_header_host->advertising_router; });
	if (it != router_lsas.end()) {
		router_lsas.erase(it);
	}
	/* 插入新 LSA */
	LSARouter* lsa_router_net = (LSARouter*)data;
	LSARouter* lsa_router_host = new LSARouter;
	lsa_router_host->header	= *lsa_header_host;
	lsa_router_host->zero0	= 0;
	lsa_router_host->b_v		= lsa_router_net->b_v;
	lsa_router_host->b_e		= lsa_router_net->b_e;
	lsa_router_host->b_b		= lsa_router_net->b_b;
	lsa_router_host->zero1	= 0;
	lsa_router_host->links_num = ntohs(lsa_router_net->links_num);
	LSARouterLink* link_net = (LSARouterLink*)((char*)data + sizeof(LSAHeader) + sizeof(LSARouter));
	for (int i = 0; i < lsa_router_host->links_num; ++i) {
		LSARouterLink link;
		link.link_id	= ntohl(link_net->link_id);
		link.link_data	= ntohl(link_net->link_data);
		link.type		= link_net->type;
		link.tos_num	= link_net->tos_num;
		link.metric		= ntohs(link_net->metric);
		lsa_router_host->links.push_back(link);
		if (link.tos_num != 0) {
			perror("TOS not zero\n");
		}
		++link_net;
	}
	delete lsa_router_host;
	delete lsa_header_host;
}

void LSDB::install_network_lsa(void *data) {
	LSANetwork* lsa_network_net = (LSANetwork*)data;
	LSANetwork* lsa_network_host = new LSANetwork;
	lsa_network_host->header = *LSAHeader::ntoh(&lsa_network_net->header);

	/* 删除旧 LSA */
	auto it = std::find_if(network_lsas.begin(), network_lsas.end(), [lsa_network_host](LSANetwork* lsa) {
		return lsa->header.link_state_id == lsa_network_host->header.link_state_id && lsa->header.advertising_router == lsa_network_host->header.advertising_router; });
	if (it != network_lsas.end()) {
		network_lsas.erase(it);
	}
	/* 插入新 LSA */
	uint32_t* network_router = (uint32_t*)((char *)data + sizeof(LSAHeader) + sizeof(uint32_t));
	uint32_t* network_router_end = (uint32_t*)((char *)data + lsa_network_host->header.length);
	for (; network_router != network_router_end; ++network_router) {
		uint32_t network_router_host;
		network_router_host = ntohl(*network_router);
		lsa_network_host->attached_routers.push_back(network_router_host);
	}
	network_lsas.push_back(lsa_network_host);
	delete lsa_network_host;
}

uint16_t fletcher16(const uint8_t* data, size_t len) { // form wikipedia
	uint16_t sum1 = 0;
	uint16_t sum2 = 0;

	for (size_t index = 0; index < len; ++index) {
		sum1 = (sum1 + data[index]) % 255;
		sum2 = (sum2 + sum1) % 255;
	}

	return (sum2 << 8) | sum1;
}

uint16_t fletcher16_ru(const void* data, size_t len, int checksum_offset) {
    const uint8_t* ptr = static_cast<const uint8_t*>(data);
    int length = len;

    int32_t x, y;
	uint32_t mul, c0 = 0, c1 = 0;
	// debugf("lsa context: ");
	for (int index = 0; index < length; index++) {
		// debugf("%02x ", *ptr);
		c0 = c0 + *(ptr++);
		c1 += c0;
	}
	// debugf("\n");

	c0 = c0 % 255;
	c1 = c1 % 255;

	// 正常结束
	// 后面部分什么意思还没搞清楚

    mul = (length - checksum_offset)*(c0);
  
	x = mul - c0 - c1;
	y = c1 - mul - 1;

	if ( y >= 0 ) y++;
	if ( x < 0 ) x--;

	x %= 255;
	y %= 255;

	if (x == 0) x = 255;
	if (y == 0) y = 255;

	y &= 0x00FF;
  
	return (x << 8) | y;
}

uint16_t lsa_checksum(const LSAHeader* lsa_header, const uint16_t length) {
	// debugf("lsa_checksum: length = %d\n", length);
	void *p = malloc(length);
	memcpy(p, lsa_header, length);
	LSAHeader* lsa_header_copy = (LSAHeader*)p;
	lsa_header_copy->ls_checksum = 0;

	uint16_t checksum = fletcher16_ru((uint8_t*)p + 2, length - 2, 14);
	free(p);
	return checksum;
}

LSDB::LSDB() {
	pthread_create(&aging_thread, nullptr, lsdb_aging_thread, (void*)this);
}

void* lsdb_aging_thread(void *data) {
	while(true) {
		sleep(1);
		OSPFArea* area = (OSPFArea*)data;
		LSDB* lsdb = &area->lsdb;
		// pthread_mutex_lock(&lsdb->lsa_mutex);
		for (auto it : lsdb->router_lsas) {
			if (it->header.ls_age < 0xff) {
				it->header.ls_age++;
			}
		}
		for (auto it : lsdb->network_lsas) {
			if (it->header.ls_age < 0xff) {
				it->header.ls_age++;
			}
		}
		// pthread_mutex_unlock(&lsdb->lsa_mutex);
	}
	pthread_exit(NULL);
}

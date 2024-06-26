#include <net/if.h>
#include <netinet/ip.h>
#include <bits/stdc++.h>
#include <sys/socket.h>
#include <netinet/ether.h>
#include <netinet/in.h>
#include <stdio.h>
#include <netdb.h>
#include <string.h>
#include <errno.h>
#include <unistd.h>
#include <sys/ioctl.h> 
#include <arpa/inet.h>

#include "ospf.h"
#include "config.h"
#include "interface.h"
#include "debug.h"
#include "link_state.h"

extern GlobalConfig myconfig;

struct protoent *proto_ospf;

int ospf_init() {
	proto_ospf = getprotobyname("ospf");
	if (proto_ospf == NULL) {
		printf("getprotobyname() failed, use default value.\n");
		proto_ospf = new protoent();
		proto_ospf->p_proto = 89;
		// printf("%d\b", proto_ospf->p_proto);
	}
	return 0;
}

/**
 * @brief 计算ospf数据包的校验和
 * 详见 [RFC2328 D.4](https://www.rfc-editor.org/rfc/rfc2328#page-231)
 * 
 * @param data 数据包的指针
 * @param len 数据包的长度
 * @return uint16_t 校验和
*/
uint16_t ospf_checksum(const void* data, size_t len) {
	uint32_t sum = 0;
	/* 计算十六位二进制和 */
    const uint16_t* ptr = static_cast<const uint16_t*>(data);
	for (size_t i = 0; i < len / 2; ++i) {
        sum += *ptr;
		++ptr;
    }
	/* 如果长度是奇数，最后一个字节单独处理 */
    if (len & 1) {
        sum += static_cast<const uint8_t*>(data)[len - 1];
    }
	/* 处理进位 */
    while (sum >> 16) {
        sum = (sum & 0xffff) + (sum >> 16);
    }
    return static_cast<uint16_t>(~sum);
}

/**
 * @brief 发送 ospf 数据包
 * 
 * @param dst_ip 		目的 ip
 * @param ospf_type 	ospf 类型
 * @param ospf_data		ospf 数据
 * @param ospf_data_len ospf 数据长度
 */
void send_ospf_packet(uint32_t dst_ip,
	const uint8_t ospf_type, const char* ospf_data, const size_t ospf_data_len,
	Interface* interface) {

	int socket_fd;
    // if ((socket_fd = socket(AF_INET, SOCK_RAW, proto_ospf->p_proto)) < 0) {
    //     perror("SendPacket: socket_fd init");
    // }
    if ((socket_fd = socket(AF_INET, SOCK_RAW, 89)) < 0) {
        perror("SendPacket: socket_fd init");
    }

	/* 将 socket 绑定到特定到网卡，所有的包都将通过该网卡进行发送 */
	/* 在 linux 中，所有网络设备都使用 ifreq 来标识 */
	/* 在此处只需要使用 ifreq 的 ifr_name */
	/* 后续是不是改为使用 interface 的 ip */
	struct ifreq ifr;
	memset(&ifr, 0, sizeof(ifr));
	strcpy(ifr.ifr_name, myconfig.nic_name);
    if (setsockopt(socket_fd, SOL_SOCKET, SO_BINDTODEVICE, &ifr, sizeof(ifr)) < 0) {
        perror("SendPacket: setsockopt");
    }

	/* 1480 = 1500(MTU) - 20(IP_HEADER) */
	char* ospf_packet = (char*)malloc(1480);
	/* 设置 ospf header */
	size_t ospf_len = ospf_data_len + sizeof(OSPFHeader);
	OSPFHeader* ospf_header 	= (OSPFHeader*)ospf_packet;
    ospf_header->version		= 2;
    ospf_header->type 			= ospf_type;
    ospf_header->packet_length 	= htons(ospf_len);
    ospf_header->router_id 		= htonl(myconfig.router_id);
    ospf_header->area_id   		= htonl(interface->area->id);
    ospf_header->checksum 		= 0;
    ospf_header->autype 		= interface->au_type;
	if (interface->au_type == 0) {
		ospf_header->authentication[0] = 0;
		ospf_header->authentication[1] = 0;
	} else {
		perror("SendPacket: Unsupport Authentication Type\n");
	}

	/* 复制 ospf 数据部分 */
	memcpy(ospf_packet + sizeof(OSPFHeader), ospf_data, ospf_data_len);

	/* 计算校验和 */
	ospf_header->checksum 		= ospf_checksum(ospf_packet, ospf_len);

	/* 设置目的地址，使用 sockaddr_in */
	/* sendto 中需要将 sockaddr_in 强制类型转换为 sockaddr */
    struct sockaddr_in dst_sockaddr;
    memset(&dst_sockaddr, 0, sizeof(dst_sockaddr));
    dst_sockaddr.sin_family = AF_INET;
    dst_sockaddr.sin_addr.s_addr = htonl(dst_ip);

	/* 发送 ospf 包 */
	if (sendto(socket_fd, ospf_packet, ospf_len, 0, (struct sockaddr*)&dst_sockaddr, sizeof(dst_sockaddr)) < 0) {
		perror("SendPacket: sendto");
	}
	else {
		printf("SendPacket: type %d to %s send success, len %ld\n", ospf_type, inet_ntoa(dst_sockaddr.sin_addr), ospf_data_len);
	}
	free(ospf_packet);
}

void* send_ospf_hello_packet_thread(void* inter) {
	Interface *interface = (Interface*)inter;

	int socket_fd;
    if ((socket_fd = socket(AF_INET, SOCK_RAW, proto_ospf->p_proto)) < 0) {
        perror("SendHelloPacket: socket_fd init");
    }

	/* 将 socket 绑定到特定到网卡，所有的包都将通过该网卡进行发送 */
	/* 在 linux 中，所有网络设备都使用 ifreq 来标识 */
	/* 在此处只需要使用 ifreq 的 ifr_name */
	struct ifreq ifr;
	memset(&ifr, 0, sizeof(ifr));
	strcpy(ifr.ifr_name, myconfig.nic_name);
    if (setsockopt(socket_fd, SOL_SOCKET, SO_BINDTODEVICE, &ifr, sizeof(ifr)) < 0) {
        perror("SendHelloPacket: setsockopt");
    }

	/* 将目的地址设为 AllSPFRouters 224.0.0.5 */
    struct sockaddr_in dst_sockaddr;
    memset(&dst_sockaddr, 0, sizeof(dst_sockaddr));
    dst_sockaddr.sin_family = AF_INET;
    dst_sockaddr.sin_addr.s_addr = inet_addr("224.0.0.5");

	/* 1480 = 1500(MTU) - 20(IP_HEADER) */
	char* ospf_packet = (char*) malloc(1480);
	while (true) {
		size_t ospf_len = sizeof(OSPFHeader) + sizeof(OSPFHello);
		OSPFHeader* ospf_header 	= (OSPFHeader*)ospf_packet;
		ospf_header->version		= 2;
		ospf_header->type 			= 1;
		ospf_header->router_id 		= htonl(myconfig.router_id);
		ospf_header->area_id   		= htonl(interface->area->id);
		ospf_header->checksum 		= 0;
		ospf_header->autype 		= interface->au_type;
		if (interface->au_type == 0) {
			ospf_header->authentication[0] = 0;
			ospf_header->authentication[1] = 0;
		} else {
			perror("SendPacket: Unsupport Authentication Type\n");
		}
		OSPFHello* ospf_hello = (OSPFHello*)(ospf_packet + sizeof(OSPFHeader)); 
        ospf_hello->ip_interface_mask			= htonl(interface->ip_interface_mask);	// 对应接口的 ip_interface_mask
        ospf_hello->hello_interval				= htons(myconfig.hello_interval);
        ospf_hello->options						= 0x02;
        ospf_hello->rtr_pri						= 1;
        ospf_hello->router_dead_interval		= htonl(myconfig.dead_interval);
        ospf_hello->designated_router			= htonl(interface->dr);
        ospf_hello->backup_designated_router	= htonl(interface->bdr);
		uint32_t* ospf_hello_neighbor = (uint32_t*)(ospf_packet + sizeof(OSPFHeader) + sizeof(OSPFHello));
        for (auto nbr: interface->neighbors) {
			if (nbr->state == NeighborState::S_DOWN) {
				continue;
			}
            *ospf_hello_neighbor = htonl(nbr->id);
			ospf_len += sizeof(uint32_t);
			++ospf_hello_neighbor;
        }
		/* 计算校验和 */
		ospf_header->packet_length 	= htons(ospf_len);
		ospf_header->checksum 		= ospf_checksum(ospf_packet, ospf_len);
		/* Send Packet */
		if (sendto(socket_fd, ospf_packet, ospf_len, 0, (struct sockaddr*)&dst_sockaddr, sizeof(dst_sockaddr)) < 0) {
			perror("SendHelloPacket: sendto");
		}
		else {
			debugf("SendHelloPacket: send success\n");
		}
        sleep(myconfig.hello_interval);
	}
	free(ospf_packet);
}

void* recv_ospf_packet_thread(void *inter) {
	Interface *interface = (Interface*)inter;
	int socket_fd;
	if ((socket_fd = socket(AF_PACKET, SOCK_RAW, htons(ETH_P_IP))) < 0) {
        perror("[Thread]RecvPacket: socket_fd init");
    }
	struct ifreq ifr;
	memset(&ifr, 0, sizeof(ifr));
	strcpy(ifr.ifr_name, myconfig.nic_name);
    if (setsockopt(socket_fd, SOL_SOCKET, SO_BINDTODEVICE, &ifr, sizeof(ifr)) < 0) {
        perror("RecvPacket: setsockopt");
    }

	char* frame = (char*)malloc(1514);
	char* packet;
    struct iphdr *ip_header;
    struct in_addr src, dst;
	
	while (true) {
		/* Hello Timer */
		if (interface->state == InterfaceState::S_WAITING && interface->waiting_timeout == true) {
			interface->event_wait_timer();
		}
		timespec now;
		clock_gettime(CLOCK_REALTIME, &now);
		for (auto neighbor: interface->neighbors) {
			if (neighbor->state == NeighborState::S_DOWN) {
				continue;
			}
			if (neighbor->last_recv_hello_time.tv_sec + interface->router_dead_interval < now.tv_sec) {
				neighbor->event_inactivity_timer();
				interface->event_neighbor_change();
			}
		}
		/* Recv frame */
		recv(socket_fd, frame, 1514, 0);
		packet = frame + sizeof(struct ethhdr);
		ip_header = (struct iphdr*)packet;
		/* IP 校验和错误的包已被丢弃 */
		/* 目的地址必须是接收接口地址或者是多播地址 */
		/* ALLSPFRouters	224.0.0.5 */
		/* ALLDRouter		224.0.0.6 */
		dst.s_addr = ip_header->daddr;
		if (dst.s_addr != htonl(interface->ip_interface_address)
		&& dst.s_addr != inet_addr("224.0.0.5")
		&& dst.s_addr != inet_addr("224.0.0.6")) {
			continue;
		}
		/* 丢弃非 OSPF 包 */
		if (ip_header->protocol != proto_ospf->p_proto) {
			continue;
		}
		/* 检查发送的对象是否是本机 */
		src.s_addr = ip_header->saddr;
		if (src.s_addr == htonl(interface->ip_interface_address)) {
			continue;
		}
		/* 校验 OSPF 包头 */
		OSPFHeader* ospf_header = (OSPFHeader*)(packet + sizeof(struct iphdr));
		if (ospf_header->version != 2) {
			continue;
		}
		if (ospf_header->area_id != htonl(interface->area->id)) {
			/* 源和目的接口是否在同一网络上校验略过 */
			continue;
		}
		/* 非 DR 和 BDR 不处理发往 ALLDRouter 的包 */
		if (dst.s_addr == inet_addr("224.0.0.6")) {
			if (interface->state != InterfaceState::S_DR && interface->state != InterfaceState::S_BACKUP) {
				continue;
			}
		}
		/* 仅支持空验证 */
		if (ospf_header->autype != interface->au_type) {
			continue;
		}
		/* 接收到一个合法的 OSPF 包 */
		debugf("RecvPacket: from %s", inet_ntoa(src));
		/* 处理 Hello 包 */
		if (ospf_header->type == T_HELLO) {
			debugf(" Hello packet\n");
			OSPFHello* ospf_hello = (OSPFHello*)(packet + sizeof(struct iphdr) + sizeof(OSPFHeader));
			/* 检查选项参数是否匹配 */
			if (ospf_hello->options != myconfig.ospf_options) {
				debugf("RecvPacket: Options mismatch\n");
				continue;
			}
			if (ospf_hello->hello_interval != htons(interface->hello_interval)
			|| ospf_hello->router_dead_interval != htonl(interface->router_dead_interval)) {
				debugf("RecvPacket: Interval mismatch\n");
				continue;
			}
			uint32_t src_ip = ntohl(src.s_addr);
			// uint32_t dst_ip = ntohl(dst.s_addr);
			auto neighbor = interface->find_neighbor(src_ip);
			if (neighbor == nullptr) {
				Neighbor* neig = new Neighbor(src_ip);
				interface->add_neighbor(neig);
				debugf("RecvPacket: Add %s into neighbors\n", n_ip2string(ospf_header->router_id));
				neighbor = interface->find_neighbor(src_ip);
			}
			bool new_neighbor = false;
			if (neighbor->state == NeighborState::S_DOWN) {
				new_neighbor = true;

			}
			/* 收到 Hello 包 */
			neighbor->event_hello_received();
			uint32_t pre_dr		= neighbor->dr;
			uint32_t pre_bdr	= neighbor->bdr;
			uint8_t  pre_pri	= neighbor->pri;
			// 检查 options
			neighbor->id	= ntohl(ospf_header->router_id);
			neighbor->dr	= ntohl(ospf_hello->designated_router);
			neighbor->bdr	= ntohl(ospf_hello->backup_designated_router);
			neighbor->pri	= ospf_hello->rtr_pri;
			neighbor->options = ospf_hello->options;
			/* 检查自己的 router id 是否在 Hello 报文中 */
			uint32_t* ospf_hello_neighbor 	= (uint32_t*)(packet + sizeof(struct iphdr) + sizeof(OSPFHeader) + sizeof(OSPFHello));
			uint32_t* ospf_end 				= (uint32_t*)(packet + sizeof(struct iphdr) + ntohs(ospf_header->packet_length));
            bool b_2way = false;
			for (;ospf_hello_neighbor != ospf_end; ++ospf_hello_neighbor) {
				if (*ospf_hello_neighbor == htonl(myconfig.router_id)) {
					b_2way = true;
					break;
				}
            }
			if (b_2way) {
				neighbor->event_2way_received();
			} else {
				neighbor->event_1way_received();
				continue;
			}
			/* 检查事件 */
			if (pre_pri != neighbor->pri && !new_neighbor) {
				interface->event_neighbor_change();
			} else if (neighbor->dr == neighbor->ip && neighbor->bdr == 0x00000000 && interface->state == InterfaceState::S_WAITING) {
				interface->event_backup_seen();
			} else if (pre_dr != neighbor->ip && neighbor->dr == neighbor->ip && !new_neighbor) {
				interface->event_neighbor_change();
			} else if (pre_dr == neighbor->ip && neighbor->dr != neighbor->ip && !new_neighbor) {
				interface->event_neighbor_change();
			} else if (neighbor->bdr == neighbor->ip && interface->state == InterfaceState::S_WAITING) {
				interface->event_backup_seen();
			} else if (pre_bdr != neighbor->ip && neighbor->bdr == neighbor->ip && !new_neighbor) {
				interface->event_neighbor_change();
			} else if (pre_bdr == neighbor->ip && neighbor->bdr != neighbor->ip && !new_neighbor) {
				interface->event_neighbor_change();
			} 
		} else if (ospf_header->type == T_DD) {
			debugf(" DD packet\n");
			OSPFDD* ospf_dd = (OSPFDD*)(packet + sizeof(iphdr) + sizeof(OSPFHeader));
			Neighbor* neighbor = interface->find_neighbor(ntohl(src.s_addr));
			if (neighbor == nullptr) {
				continue;
			}
			bool is_dumplicated = false;
			uint32_t seq_num = ntohl(ospf_dd->dd_seq_num);
            if (seq_num == neighbor->last_recv_dd_seq_num
			&& ospf_dd->b_I == neighbor->last_recv_dd_i
			&& ospf_dd->b_M == neighbor->last_recv_dd_m
			&& ospf_dd->b_MS == neighbor->last_recv_dd_ms) {
				is_dumplicated = true;
            } else {
                neighbor->last_recv_dd_seq_num 	= seq_num;
                neighbor->last_recv_dd_i 		= ospf_dd->b_I;
                neighbor->last_recv_dd_m 		= ospf_dd->b_M;
                neighbor->last_recv_dd_ms 		= ospf_dd->b_MS;
            }
			neighbor->options = ospf_dd->options;

			DD_BEGIN:
			if (neighbor->state == NeighborState::S_DOWN || neighbor->state == NeighborState::S_ATTEMPT) {
				debugf("DD packet REJECT (DOWN)\n");
				continue;
			} else if (neighbor->state == NeighborState::S_INIT) {
				neighbor->event_2way_received();
				/* 如果改变状态，重新处理该包 */
				if (neighbor->state == NeighborState::S_EXSTART) {
					goto DD_BEGIN;
				}
				continue;
			} else if (neighbor->state == NeighborState::S_2WAY) {
				debugf("DD packet IGNORE (2WAY)\n");
				continue;
			} else if (neighbor->state == NeighborState::S_EXSTART) {
				/* neighbor is master */
				if (ospf_dd->b_I == 1 && ospf_dd->b_M == 1 && ospf_dd->b_MS == 1
				&& neighbor->id > myconfig.router_id) {
					neighbor->is_master = true;
					neighbor->dd_seq_num = seq_num;
					neighbor->event_negotiation_done();
					debugf("neighbor is master\n");
				}
				/* neighbor is slave，*/
				else if (ospf_dd->b_I == 0 && ospf_dd->b_MS == 0
				&& seq_num == neighbor->dd_seq_num
				&& neighbor->id < myconfig.router_id) {
					neighbor->is_master = false;
					neighbor->event_negotiation_done();
				}
				else {
					debugf("DD packet IGNORE (S_EXSTART)\n");
					continue;
				}
			} else if (neighbor->state == NeighborState::S_EXCHANGE) {
				/* 重复包 */
				if (is_dumplicated) {
					if (neighbor->is_master) {
						send_ospf_packet(neighbor->ip, T_DD, neighbor->last_send_dd_data, neighbor->last_send_dd_data_len, interface);
					} else {
						debugf("DD packet IGNORE for receiving dumplicated packet. (S_EXCHANGE)\n");
					}
					continue;
				}
				/* 选项不同 */
				/* UPDATE：某些选项的不同是可以接受的，比方说 O 位 */
				uint8_t options = OPTION_E | OPTION_NP;
				if ((ospf_dd->options & options) != (neighbor->options & options)) {
					debugf("DD packet REJECT for options mismatch\n");
					neighbor->event_seq_number_mismatch();
					continue;
				}
				/* 意外设置了 I 位 */
				if (ospf_dd->b_I == 1) {
					debugf("DD packet REJECT for unexpected I bit\n");
					neighbor->event_seq_number_mismatch();
					continue;
				}
				/* MS 与主从关系不匹配或者 dd_seq_num 不匹配 */
				if (neighbor->is_master) {
					if (ospf_dd->b_MS == 0 || seq_num != neighbor->dd_seq_num + 1) {
						debugf("DD packet REJECT for MS or seq_num mismatch\n");
						neighbor->event_seq_number_mismatch();
						continue;
					}
				} else {
					if (ospf_dd->b_MS == 1 || seq_num != neighbor->dd_seq_num) {
						debugf("DD packet REJECT for MS or seq_num mismatch\n");
						neighbor->event_seq_number_mismatch();
						continue;
					}
				}
			} else if (neighbor->state == NeighborState::S_LOADING || neighbor->state == NeighborState::S_FULL) {
				/* 只可能接收重复包 */
				if (is_dumplicated) {
					if (neighbor->is_master) {
						/* TODO 如果超时，则生成 seq_number_mismatch */
						send_ospf_packet(neighbor->ip, T_DD, neighbor->last_send_dd_data, neighbor->last_send_dd_data_len, interface);
					} else {
						debugf("DD packet IGNORE for receiving dumplicated packet. (S_LOADING / S_FULL)\n");
					}
				} else {
					debugf("DD packet IGNORE for not dumplicating packet. (S_LOADING / S_FULL)\n");
				}
				continue;
			} else {
				debugf("DD packet REJECT for not support state (%d)\n", neighbor->state);
				continue;
			}
			
			/* 接收到一个序号匹配的 DD 包 */
			if (neighbor->is_master) {
				neighbor->dd_seq_num = seq_num;
			}
			LSAHeader* lsa_header 		= (LSAHeader*)(packet + sizeof(iphdr) + sizeof(OSPFHeader) + sizeof(OSPFDD));
			LSAHeader* lsa_header_end	= (LSAHeader*)(packet + sizeof(iphdr) + ntohs(ospf_header->packet_length));
			/* 检查 lsa_type */
			for (; lsa_header != lsa_header_end; ++lsa_header) {
				if (lsa_header->ls_type < 1 || lsa_header->ls_type > 5) {
					debugf("DD packet REJECT for invalid LSA Header\n");
					neighbor->event_seq_number_mismatch();
					continue;
				}
				/* TODO ls_type == 5 && 邻居关联到存根区域 */
				// if (lsa_header->ls_type == 5)
			}
			/* 对比 dd 和 lsdb 中的 LSA Header */
			LSDB* lsdb = &interface->area->lsdb;
			lsa_header = (LSAHeader*)(packet + sizeof(iphdr) + sizeof(OSPFHeader) + sizeof(OSPFDD));
			pthread_mutex_lock(&neighbor->lsr_mutex);
			// pthread_mutex_lock(&lsdb->lsa_mutex);
			for (; lsa_header != lsa_header_end; ++lsa_header) {
				LSAHeader* header = new LSAHeader;
				header->ls_age 				= ntohs(lsa_header->ls_age);
				header->options 			= lsa_header->options;
				header->ls_type 			= lsa_header->ls_type;
				header->link_state_id 		= ntohl(lsa_header->link_state_id);
				header->advertising_router 	= ntohl(lsa_header->advertising_router);
				header->ls_seq_num 			= ntohl(lsa_header->ls_seq_num);
				header->ls_checksum 		= ntohs(lsa_header->ls_checksum);
				header->length 				= ntohs(lsa_header->length);
				if (header->ls_type == LSA_ROUTER) {
					LSARouter* lsa_router = lsdb->find_router_lsa(header->link_state_id, header->advertising_router);
					if (lsa_router == nullptr) {
						neighbor->link_state_request_list.push_back(*header);
					} else {
						if (lsa_header_cmp(header, &lsa_router->header) < 0) {
							neighbor->link_state_request_list.push_back(*header);
						}
					}
				} else if (header->ls_age == LSA_NETWORK) {
					LSANetwork* lsa_network = lsdb->find_network_lsa(header->link_state_id, header->advertising_router);
					if (lsa_network == nullptr) {
						neighbor->link_state_request_list.push_back(*header);
					} else {
						if (lsa_header_cmp(header, &lsa_network->header) < 0) {
							neighbor->link_state_request_list.push_back(*header);
						}
					}
				} else {
					debugf("DD packet REJECT for invalid LSA Header\n");
				}
				/* list 中存的是拷贝，直接 delete */
				delete header;
			}
			// pthread_mutex_unlock(&lsdb->lsa_mutex);
			pthread_mutex_unlock(&neighbor->lsr_mutex);
			/* 确认前一个数据包时，将已发送的 LSAHeader 从 database summary list 中删除 */
			if (neighbor->last_send_dd_data_len != 0) {
				int cnt = (neighbor->last_send_dd_data_len - sizeof(OSPFDD)) / sizeof(LSAHeader);
				while (cnt) {
					neighbor->database_summary_list.pop_front();
					--cnt;
				}
			}
			/* 发送一个 DD 包 */
			if (ospf_dd->b_M == 0 && neighbor->database_summary_list.empty()) {
				neighbor->event_exchange_done();
			}
			char* send_dd_data = (char*)malloc(1024);
			uint32_t send_dd_data_len = sizeof(OSPFDD);
			OSPFDD* send_ospf_dd = (OSPFDD*)send_dd_data;
			send_ospf_dd->interface_mtu = htons(neighbor->interface->mtu);
			send_ospf_dd->options		= myconfig.ospf_options;
			send_ospf_dd->b_I			= 0;
			if (neighbor->is_master) {
				send_ospf_dd->b_MS = 0;
			} else {
				neighbor->dd_seq_num++;
				send_ospf_dd->b_MS = 1;
			}
			send_ospf_dd->dd_seq_num = htonl(neighbor->dd_seq_num);
			LSAHeader* header = (LSAHeader*)(send_dd_data + sizeof(OSPFDD));
			bool more = false;
			for (auto lsa_header: neighbor->database_summary_list) {
				if (send_dd_data_len + sizeof(LSAHeader) > 1024) {
					more = true;
					break;
				}
				memcpy(header, LSAHeader::hton(&lsa_header), sizeof(LSAHeader));
				++header;
				send_dd_data_len += sizeof(LSAHeader);
			}
			send_ospf_dd->b_M = more;
			send_ospf_packet(neighbor->ip, T_DD, send_dd_data, send_dd_data_len, interface);
			memcpy(neighbor->last_send_dd_data, send_dd_data, send_dd_data_len);
			neighbor->last_send_dd_data_len = send_dd_data_len;
			free(send_dd_data);
		} else if (ospf_header->type == T_LSR) {
			/* DR 和 BDR 向多播地址 ALLSPFRouters 发送 LSU 和 LSAck */
			/* 其他路由器向多播地址 ALLDRouters 发送 LSU 和 LSAck */
			/* 重传的 LSU 包直接被发往邻居 */
			debugf(" LSR packet\n");
			Neighbor* neighbor = interface->find_neighbor(ntohl(src.s_addr));
			assert(neighbor != nullptr);
			if (neighbor->state != NeighborState::S_EXCHANGE && neighbor->state != NeighborState::S_LOADING && neighbor->state != NeighborState::S_FULL) {
				debugf(" LSR packet REJECT for neighbor state\n");
				continue;
			}

			OSPFLsr* ospf_lsr 		= (OSPFLsr*)(packet + sizeof(iphdr) + sizeof(OSPFHeader));
			OSPFLsr* ospf_lsr_end	= (OSPFLsr*)(packet + sizeof(iphdr) + ntohs(ospf_header->packet_length));
			LSDB* lsdb = &interface->area->lsdb;
			bool bad_ls_req = false;
			char* lsu_data = (char*)malloc(1024);
			OSPFLsu* lsu = (OSPFLsu*)lsu_data;
			lsu->num = 0;
			char* lsa = lsu_data + sizeof(OSPFLsu);
			// pthread_mutex_lock(&lsdb->lsa_mutex);
			for (; ospf_lsr != ospf_lsr_end; ++ospf_lsr) {
				uint32_t ls_type			= ntohl(ospf_lsr->ls_type);
				uint32_t link_state_id		= ntohl(ospf_lsr->link_state_id);
				uint32_t advertising_router = ntohl(ospf_lsr->advertising_router);

				if (ls_type == LSA_ROUTER) {
					LSARouter* router_lsa = lsdb->find_router_lsa(link_state_id, advertising_router);
					if (router_lsa == nullptr) {
						neighbor->event_bad_ls_req();
						bad_ls_req = true;
						break;
					}
					LSARouter* router_lsa_net = (LSARouter*)lsa;
					memcpy((LSAHeader*)router_lsa_net, LSAHeader::ntoh(&router_lsa->header), sizeof(LSAHeader));
					router_lsa_net->zero0	= router_lsa->zero0;
					router_lsa_net->b_v		= router_lsa->b_v;
					router_lsa_net->b_e		= router_lsa->b_e;
					router_lsa_net->b_b		= router_lsa->b_b;
					router_lsa_net->zero1	= router_lsa->zero1;
					router_lsa_net->links_num = htons(router_lsa->links_num);
					lsa += sizeof(LSAHeader) + sizeof(uint8_t) * 2 + sizeof(uint16_t);
					for (auto link : router_lsa->links) {
						LSARouterLink* router_link = (LSARouterLink*)lsa;
						router_link->link_id	= htonl(link.link_id);
						router_link->link_data	= htonl(link.link_data);
						router_link->type		= link.type;
						router_link->tos_num	= link.tos_num;
						router_link->metric		= htons(link.metric);
						if (link.tos_num != 0) {
							perror("TODO: TOS\n");
						}
						lsa += sizeof(LSARouterLink);
					}
					router_lsa_net->header.ls_checksum = htons(lsa_checksum((LSAHeader*)router_lsa_net, htons(router_lsa_net->header.length)));
				} else if (ls_type == LSA_NETWORK) {
					LSANetwork* lsa_network = lsdb->find_network_lsa(link_state_id, advertising_router);
					if (lsa_network == nullptr) {
						neighbor->event_bad_ls_req();
						bad_ls_req = true;
						break;
					}
					memcpy(lsa, LSAHeader::ntoh(&lsa_network->header), sizeof(LSAHeader));
					lsa += sizeof(LSAHeader);
					*(uint32_t*)lsa = htonl(lsa_network->network_mask);
					lsa += sizeof(uint32_t);
					for (auto attach_router : lsa_network->attached_routers) {
						*(uint32_t*)lsa = htonl(attach_router);
						lsa += sizeof(uint32_t);
					}
				} else {
					perror("LSR: lsa type no complement!");
				}
				lsu->num = htons(ntohs(lsu->num) + 1);
			}
			// pthread_mutex_unlock(&lsdb->lsa_mutex);
			if (!bad_ls_req) {
				send_ospf_packet(neighbor->ip, T_LSU, lsu_data, lsa - lsu_data, interface);
			}
			free(lsu_data);
		} else if (ospf_header->type == T_LSU) {
			debugf(" LSU packet\n");
			OSPFLsu* ospf_lsu = (OSPFLsu*)(packet + sizeof(iphdr) + sizeof(OSPFHeader));
			int lsa_num = ntohl(ospf_lsu->num);

			Neighbor* neighbor = interface->find_neighbor(ntohl(src.s_addr));
			assert(neighbor != nullptr);

			LSDB* lsdb = &interface->area->lsdb;
			
			char* lsa_header_ptr = (char*)(packet + sizeof(iphdr) + sizeof(OSPFHeader) + sizeof(OSPFLsu));
			LSAHeader* lsa_header;
			LSAHeader* lsa_header_host = new LSAHeader();

			char* lsack_data = (char*)malloc(1024);
            LSAHeader* lsack_lsa_header = (LSAHeader*)lsack_data;
			bool bad_ls_req = false;

			/* 接收到的 LSU 包有三种可能 */
			/* 一种组播，用于宣告新信息 */
			/* 一种直接发向邻居，用于重传 flooding */
			/* 一种直接发向邻居，用于同步邻居 */
			/* 两者通过 dst.ip 和 state 进行区分 */
			// pthread_mutex_lock(&lsdb->lsa_mutex);
			for (int i = 0; i < lsa_num; ++i, lsa_header_ptr += ntohs(lsa_header->length)) {
				lsa_header = (LSAHeader*)lsa_header_ptr;
				memcpy(lsa_header_host, LSAHeader::ntoh((LSAHeader*)lsa_header_ptr), sizeof(LSAHeader));
				/* 1. 检查 LS CHECKSUM */
				if (lsa_header->ls_checksum != htons(lsa_checksum(lsa_header, htons(lsa_header->length)))) {
					debugf("LSU packet REJECT for checksum error\n");
					continue;	
				}
				/* 2. 检查 LS TYPE */
				if (lsa_header_host->ls_type < 1 || lsa_header_host->ls_type > 5) {
					continue;
				}
				/* 3. TODO 检查是否是外部 LSA */
				if (lsa_header_host->ls_type == LSA_ASEXTERNAL) {
					perror("TODO: LSA_ASEXTERNAL\n");
				}
				/* 4. 如果 LSA 的 LS 时限等于 MaxAge，
				而且路由器的连接状态数据库中没有该 LSA 的实例，
				而且路由器的邻居都不处于 Exchange 或 Loading 状态 */
				/* 清除 LS，且保证不影响处在同步其链路状态数据库的节点 */
				bool neighbor_loading_or_exchange = false;
				for (auto nei : interface->neighbors) {
					if (nei->state == NeighborState::S_EXCHANGE || nei->state == NeighborState::S_LOADING) {
						neighbor_loading_or_exchange = true;
						break;
					}
				}
				if (lsa_header_host->ls_age == MAX_AGE && !neighbor_loading_or_exchange) {
					/* a. 立即发送一个 LSAck 包到发送的邻居 */
					send_ospf_packet(ntohl(src.s_addr), T_LSAck, lsa_header_ptr, sizeof(LSAHeader), interface);
					/* b. 丢弃该 LSA */
					continue;
				}
				/* 5. 查找当前包含在路由器链路状态数据库中的此 LSA 的实例。
				如果没有数据库副本，或者收到的 LSA 比数据库副本更新 */
				bool lsa_exist = false;
				int lsa_cmp_result = 0;
				bool lsa_self = false;
				void* lsdb_lsa = nullptr;
				if (lsa_header_host->ls_type == LSA_ROUTER) {
					lsdb_lsa = (void*)lsdb->find_router_lsa(lsa_header_host->link_state_id, lsa_header_host->advertising_router);
					if (lsdb_lsa != nullptr) {
						LSARouter* tmp = (LSARouter*)lsdb_lsa;
						lsa_exist = true;
						lsa_cmp_result = lsa_header_cmp(lsa_header_host, &tmp->header);
						lsa_self = tmp->header.advertising_router == htonl(myconfig.router_id);
					}
				} else if (lsa_header_host->ls_type == LSA_NETWORK) {
					lsdb_lsa = (void*)lsdb->find_network_lsa(lsa_header_host->link_state_id, lsa_header_host->advertising_router);
					if (lsdb_lsa != nullptr) {
						LSANetwork* tmp = (LSANetwork*)lsdb_lsa;
						lsa_exist = true;
						lsa_cmp_result = lsa_header_cmp(lsa_header_host, &tmp->header);
						for (auto inter: interface->area->interfaces) {
							if (inter->ip_interface_address == htonl(lsa_header_host->link_state_id)) {
								lsa_self = true;
								break;
							}
						}
					}
				} else {
					perror("No complement!\n");
				}
				if (lsa_exist == false || lsa_cmp_result < 0) {
					/* a TODO 如果存在数据库副本，且通过 flooding，在不到 MinLSArrival 时间内接受，则丢弃 */
					/* 路由器不处理在 MinLSArrival 内的 LSA，从而提高稳定性 */
					/* 简化：认为所有包都在 MinLSArrival 间隔外更新 */
					if (lsa_exist == true && lsa_self == false && false) {
						continue;
					}
					/* b 立即 flooding（某些接口），见 13.3 */
					bool flooding_from_this_interface = false;
					for (auto inter: interface->area->interfaces) {
						if (inter == interface) {
							if (flooding_lsa(lsa_header_ptr, inter, true, src) == 0) {
								flooding_from_this_interface = true;
							}
						} else {
							flooding_lsa(lsa_header_ptr, inter, false, src);
						}
					}
					/* c 从所有邻居的链路状态重传列表中删除当前数据库副本（防止旧的继续被重传） */
					for (auto nei : interface->neighbors) {
						nei->link_state_retransmission_list.remove_if([&lsa_header_host](LSAHeader& lsa) {
							return lsa.link_state_id == lsa_header_host->link_state_id
								&& lsa.advertising_router == lsa_header_host->advertising_router;
						});
					}
					/* d 在链接状态数据库中安装新 LSA */
					if (lsa_header_host->ls_type == LSA_ROUTER) {
						lsdb->install_router_lsa((LSAHeader*)lsa_header_ptr);
					} else if (lsa_header_host->ls_type == LSA_NETWORK) {
						lsdb->install_network_lsa((LSARouter*)lsa_header_ptr);
					}
					/* e 可能通过向接收接口发回链路状态确认包来确认 LSA 的接收 */
					/* 13.5 如果向原接口泛洪，则不发送确认 */
					/* 否则延迟确认 */
					if (!flooding_from_this_interface) {
						if (interface->state == InterfaceState::S_BACKUP) {
							if (src.s_addr == htonl(interface->dr)) {
								memcpy(lsack_lsa_header, lsa_header_ptr, sizeof(LSAHeader));
								++lsack_lsa_header;
							}
						} else {
							memcpy(lsack_lsa_header, lsa_header_ptr, sizeof(LSAHeader));
							++lsack_lsa_header;
						}
					}
					/* f TODO 自生成 LSA 特殊操作 */
					if (lsa_self) {
						perror("TODO: self generate LSA\n");
					}
					continue;
				}

				/* 6. 如果发送邻居的链路状态请求列表上存在 LSA 实例，则数据库交换过程中发生错误 */
				for (auto header: neighbor->link_state_request_list) {
					if (header.link_state_id == lsa_header_host->link_state_id
					&& header.advertising_router == lsa_header_host->advertising_router) {
						bad_ls_req = true;
						break;
					}
				}
				if (bad_ls_req) {
					neighbor->event_bad_ls_req();
					break;
				}
				/* 7. 如果收到的LSA与数据库副本是同一实例 */
				if (lsa_cmp_result == 0) {
					/* a 如果LSA列在接收邻接的链路状态重传列表中，
					则路由器本身期望该LSA得到确认。
					路由器应通过从链路状态重传列表中删除LSA，
					将收到的LSA视为确认。
					这被称为“默示承认”。应注意其出现情况，以供确认流程稍后使用 */
					neighbor->link_state_retransmission_list.remove_if([&lsa_header_host](LSAHeader& lsa) {
						return lsa.link_state_id == lsa_header_host->link_state_id
							&& lsa.advertising_router == lsa_header_host->advertising_router;
					});
					/* b TODO 可能通过向接收接口发回链路状态确认包来确认LSA的接收 */
					/* 如果自身是 BDR，且来自 DR，则发送延迟确认 */
					if (interface->state == InterfaceState::S_BACKUP) {
						perror("TODO: delay ack\n");
					}
				}
				/* 8 此时数据库副本较新 */
				if (lsa_header_host->ls_type == LSA_ROUTER) {
					LSARouter* lsa_router = (LSARouter*)lsdb_lsa;
					if (lsa_router->header.ls_age == MAX_AGE && lsa_router->header.ls_seq_num == MAX_SEQUENCE_NUMBER) {
						continue;
					} else {
						// 发送一个 LSU 包给邻居
						perror("[TODO] send a newer LSU");
						continue;
					}
				} else if (lsa_header_host->ls_type == LSA_NETWORK) {
					lsdb->install_network_lsa((LSAHeader*)lsa_header_ptr);
					LSANetwork* lsa_network = (LSANetwork*)lsdb_lsa;
					if (lsa_network->header.ls_age == MAX_AGE && lsa_network->header.ls_seq_num == MAX_SEQUENCE_NUMBER) {
						continue;
					} else {
						// 发送一个 LSU 包给邻居
						perror("[TODO] send a newer LSU");
						continue;
					}
				} else {
					perror("No complement!\n");
				}
			}
			// pthread_mutex_unlock(&lsdb->lsa_mutex);
			size_t lsack_length = (char*)lsack_lsa_header - lsack_data;
			if (!bad_ls_req && lsack_length > 0) {
				if (interface->state == InterfaceState::S_DR || interface->state == InterfaceState::S_BACKUP) {
					send_ospf_packet(ntohl(inet_addr("224.0.0.5")), T_LSAck, lsack_data, (char*)lsack_lsa_header - lsack_data, interface);
				} else {
					send_ospf_packet(ntohl(inet_addr("224.0.0.6")), T_LSAck, lsack_data, (char*)lsack_lsa_header - lsack_data, interface);
				}
			}
			delete lsa_header_host;
			free(lsack_data);
		} else if (ospf_header->type == T_LSAck) {
			Neighbor* neighbor = interface->find_neighbor(ntohl(src.s_addr));
			assert(neighbor != nullptr);

			if (neighbor->state < NeighborState::S_EXCHANGE) {
				debugf(" LSAck packet REJECT for neighbor state\n");
				continue;
			}

			LSAHeader* lsa_header = (LSAHeader*)(packet + sizeof(iphdr) + sizeof(OSPFHeader));
			LSAHeader* lsa_header_end = (LSAHeader*)(packet + sizeof(iphdr) + ntohs(ospf_header->packet_length));
			for (; lsa_header != lsa_header_end; ++lsa_header) {
				auto lsa_save = std::find_if(neighbor->link_state_retransmission_list.begin(), neighbor->link_state_retransmission_list.end(), [lsa_header](LSAHeader& lsa) {
					return lsa.ls_type == lsa_header->ls_type
						&& lsa.link_state_id == ntohl(lsa_header->link_state_id)
						&& lsa.advertising_router == ntohl(lsa_header->advertising_router);
				});
				if (lsa_save == neighbor->link_state_retransmission_list.end()) {
					debugf(" LSAck packet REJECT for not found in retransmission list\n");
				} else {
					neighbor->link_state_retransmission_list.erase(lsa_save);
					debugf(" LSAck packet: remove from retransmission list\n");
				}
			}
		} else {
			debugf(" Unsupport packet\n");
		}
	}
	free(frame);
}


void* send_empty_dd_packet_thread(void* neigh) {
	Neighbor* neighbor = (Neighbor*)neigh;
	OSPFDD* ospf_dd = new OSPFDD();
	while (true) {
		if (neighbor->state != NeighborState::S_EXSTART || neighbor->empty_dd_sender_stop) {
            break;
        }
		memset(ospf_dd, 0, sizeof(OSPFDD));
		ospf_dd->interface_mtu = htons(neighbor->interface->mtu);
		ospf_dd->options = 0x02;
        ospf_dd->b_MS = 1;
        ospf_dd->b_M = 1;
        ospf_dd->b_I = 1;
        ospf_dd->dd_seq_num = neighbor->dd_seq_num;

		send_ospf_packet(neighbor->ip, T_DD, (char*)ospf_dd, sizeof(OSPFDD), neighbor->interface);
		debugf("SendEmptyDDPacket: Send to router id %s\n", n_ip2string(neighbor->id));
		sleep(neighbor->interface->rxmt_interval);
	}
	delete ospf_dd;
	neighbor->is_empty_dd_sender_running = false;
	pthread_exit(NULL);
}

/**
 * LSR				-> LSU(point)
 * LSU(flooding) 	-> LSAck
 * LSU(flooding) 	-> without LSAck	-> LSU(point)
 * 方案 1
 * 	发送 LSR 和接受 LSU 包放在一起
 * 	每次发送完 LSR 等待 LSU 包到达或者计时器超时后再次发送 LSR
 * 方案 2
 * 	发送 LSR 和接受 LSU 包分开
 * 	需要进程间通信
 * 如果当前 link_state_request_list 为空，说明状态已经转移，线程退出
 */
void *send_lsr_packet_thread(void* neigh) {
	Neighbor* neighbor = (Neighbor*)neigh;
	void* data = malloc(1024);
	bool is_first = true;
	struct timespec ts;
	while (true) {
		if (neighbor->state != NeighborState::S_EXCHANGE) {
			break;
		}
		pthread_mutex_lock(&neighbor->lsr_mutex);
		if (!is_first) {
			clock_gettime(CLOCK_REALTIME, &ts);
			ts.tv_sec += neighbor->interface->rxmt_interval;
			pthread_cond_timedwait(&neighbor->lsr_cond, &neighbor->lsr_mutex, &ts);
			if (neighbor->link_state_request_list.empty()) {
				pthread_mutex_unlock(&neighbor->lsr_mutex);
				break;
			}
		} else {
			is_first = false;
		}

		char* send_lsr_data = (char*)data;
		uint32_t send_lsr_data_len = 0;
		OSPFLsr* lsr = (OSPFLsr* ) send_lsr_data;
		
		for (auto& req_lsa_header: neighbor->link_state_request_list) {
			if (send_lsr_data_len + sizeof(OSPFLsr) > 1024) {
				break;
			}
			lsr->ls_type = htonl(req_lsa_header.ls_type);
			lsr->link_state_id = htonl(req_lsa_header.link_state_id);
			lsr->advertising_router = htonl(req_lsa_header.advertising_router);

			lsr++;
			send_lsr_data_len += sizeof(OSPFLsr);
		}
		send_ospf_packet(neighbor->ip, T_LSR, send_lsr_data, send_lsr_data_len, neighbor->interface);
		pthread_mutex_unlock(&neighbor->lsr_mutex);
	}
	free(data);
	return nullptr;
}

void* retrans_sender_thread(void* neigh) {
	Neighbor* neighbor = (Neighbor*)neigh;
	char* data = (char*)malloc(1024);
	LSDB* lsdb = &neighbor->interface->area->lsdb;
	while (true) {
		if (neighbor->state == NeighborState::S_DOWN) {
			break;
		}
		sleep(neighbor->interface->rxmt_interval);
		char* ptr = (char*)data;
		OSPFLsu* lsu = (OSPFLsu*)ptr;
		lsu->num = 0;
		ptr += sizeof(OSPFLsu);
		pthread_mutex_lock(&neighbor->retrans_mutex);
		for (auto lsa_header: neighbor->link_state_retransmission_list) {
			if (ptr - data + lsa_header.length > 1024) { break; }
			lsu->num = htonl(ntohl(lsu->num) + 1);
			if (lsa_header.ls_type == LSA_ROUTER) {
				LSARouter* lsa_router = lsdb->find_router_lsa(lsa_header.link_state_id, lsa_header.advertising_router);
				LSARouter* lsa_router_net = (LSARouter*)ptr;
				memcpy(&lsa_router_net->header, &lsa_router->header, sizeof(LSAHeader));
				lsa_router_net->header.ls_age = htons(lsa_router->header.ls_age + neighbor->interface->inf_trans_delay);
				lsa_router_net->zero0 = lsa_router->zero0;
				lsa_router_net->b_v = lsa_router->b_v;
				lsa_router_net->b_e = lsa_router->b_e;
				lsa_router_net->b_b = lsa_router->b_b;
				lsa_router_net->zero1 = lsa_router->zero1;
				lsa_router_net->links_num = htons(lsa_router->links_num);
				ptr += sizeof(LSAHeader) + sizeof(uint8_t) * 2 + sizeof(uint16_t);
				for (auto link: lsa_router->links) {
					LSARouterLink* router_link = (LSARouterLink*)ptr;
					router_link->link_id = htonl(link.link_id);
					router_link->link_data = htonl(link.link_data);
					router_link->type = link.type;
					router_link->tos_num = link.tos_num;
					router_link->metric = htons(link.metric);
					if (link.tos_num != 0) {
						perror("TODO: TOS\n");
					}
					ptr += sizeof(LSARouterLink);
				}
			} else if (lsa_header.ls_type == LSA_NETWORK) {
				if (ptr - data >= 900) { break; }
				LSANetwork* lsa_network = lsdb->find_network_lsa(lsa_header.link_state_id, lsa_header.advertising_router);
				LSANetwork* lsa_router_net = (LSANetwork*)ptr;
				memcpy(&lsa_router_net->header, &lsa_network->header, sizeof(LSAHeader));
				lsa_router_net->header.ls_age = htons(lsa_network->header.ls_age + neighbor->interface->inf_trans_delay);
				ptr += sizeof(LSAHeader);
				lsa_router_net->network_mask = htonl(lsa_network->network_mask);
				ptr += sizeof(uint32_t);
				for (auto attach_router: lsa_network->attached_routers) {
					*(uint32_t*)ptr = htonl(attach_router);
					ptr += sizeof(uint32_t);
				}
			} else {
				perror("Unsupport LSA type\n");
			}
			send_ospf_packet(neighbor->ip, T_LSU, data, ptr - data, neighbor->interface);
		}
		pthread_mutex_unlock(&neighbor->retrans_mutex);
	}
	free(data);
	pthread_exit(NULL);
}

int flooding_lsa(void* lsa_header_ptr, Interface* interface, bool is_origin, in_addr src) {
	LSAHeader* lsa_header_host = new LSAHeader();
	memcpy(lsa_header_host, LSAHeader::ntoh((LSAHeader*)lsa_header_ptr), sizeof(LSAHeader));

	if (lsa_header_host->ls_type == 5) {
		// TOOD
	} else {
		/* 1. 检查邻居 */
		bool add_new_lsa_transmission = false;
		for (auto nei : interface->neighbors) {
			if (nei->state < NeighborState::S_EXCHANGE) {
				continue;
			}
			if (nei->state == NeighborState::S_EXCHANGE || nei->state == NeighborState::S_LOADING) {
				auto it = std::find_if(nei->link_state_request_list.begin(), nei->link_state_request_list.end(), [&lsa_header_host](LSAHeader& lsa) {
					return lsa.link_state_id == lsa_header_host->link_state_id
						&& lsa.advertising_router == lsa_header_host->advertising_router;
				});
				if (it != nei->link_state_request_list.end()) {
					int lsa_cmp_result = lsa_header_cmp(lsa_header_host, &*it);
					if (lsa_cmp_result == 0) {
						nei->link_state_request_list.erase(it);
						continue;
					} else if (lsa_cmp_result < 0) {
						nei->link_state_request_list.erase(it);
					} else {
						continue;
					}
					if (src.s_addr == htonl(nei->ip)) {
						continue;
					} else {
						nei->link_state_retransmission_list.push_back(*lsa_header_host);
						add_new_lsa_transmission = true;
					}
				} else {
					nei->link_state_retransmission_list.push_back(*lsa_header_host);
					add_new_lsa_transmission = true;
				}
			}
		}
		/* 2. 决定是否泛洪 */
		if (add_new_lsa_transmission == true) {
			/* 3. 如果是该接口接收，且为 DR 或 BDR 发送，检查下一个接口 */
			/* 简化，只有当前接口 */
			if (src.s_addr == htonl(interface->dr) || src.s_addr == htonl(interface->bdr)) {
				/* do nothing */
			} else if (interface->state == InterfaceState::S_BACKUP) {
				/* do nothing */
			} else {
				/* 4. 向所有邻居发送 LSA */
				char *buffer = (char*)malloc(1024);
				OSPFLsu* lsu = (OSPFLsu*)buffer;
				lsu->num = htonl(1);
				LSAHeader* lsa = (LSAHeader*)(buffer + sizeof(OSPFLsu));
				memcpy(lsa, lsa_header_ptr, lsa_header_host->length);

				if (interface->state == InterfaceState::S_DR || interface->state == InterfaceState::S_BACKUP) {
					send_ospf_packet(inet_addr("224.0.0.5"), T_LSU, buffer, sizeof(OSPFLsu) + lsa_header_host->length, interface);
				} else {
					send_ospf_packet(inet_addr("224.0.0.6"), T_LSU, buffer, sizeof(OSPFLsu) + lsa_header_host->length, interface);
				}
				free(buffer);
				delete lsa_header_host;
				return 0;
			}
		} 
	}
	delete lsa_header_host;
	return -1;
}
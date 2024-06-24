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

extern GlobalConfig myconfigs;

struct protoent *proto_ospf;

int ospf_init() {
	proto_ospf = getprotobyname("ospf");
	if (proto_ospf == NULL) {
		perror("getprotobyname() failed");
		return -1;
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
    if ((socket_fd = socket(AF_INET, SOCK_RAW, proto_ospf->p_proto)) < 0) {
        perror("SendPacket: socket_fd init");
    }

	/* 将 socket 绑定到特定到网卡，所有的包都将通过该网卡进行发送 */
	/* 在 linux 中，所有网络设备都使用 ifreq 来标识 */
	/* 在此处只需要使用 ifreq 的 ifr_name */
	/* 后续是不是改为使用 interface 的 ip */
	struct ifreq ifr;
	memset(&ifr, 0, sizeof(ifr));
	strcpy(ifr.ifr_name, myconfigs.nic_name);
    if (setsockopt(socket_fd, SOL_SOCKET, SO_BINDTODEVICE, &ifr, sizeof(ifr)) < 0) {
        perror("SendPacket: setsockopt");
    }

	/* 1480 = 1500(MTU) - 20(IP_HEADER) */
	char* ospf_packet = (char*) malloc(1480);
	/* 设置 ospf header */
	size_t ospf_len = ospf_data_len + sizeof(OSPFHeader);
	OSPFHeader* ospf_header 	= (OSPFHeader*)ospf_packet;
    ospf_header->version		= 2;
    ospf_header->type 			= ospf_type;
    ospf_header->packet_length 	= htons(ospf_len);
    ospf_header->router_id 		= htonl(myconfigs.router_id);
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
		printf("SendPacket: type %d send success, len %ld\n", ospf_type, ospf_data_len);
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
	strcpy(ifr.ifr_name, myconfigs.nic_name);
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
		size_t ospf_len = sizeof(OSPFHeader) + sizeof(OSPFHello) + 4 * interface->neighbors.size();
		
		/* 头部 */
		
		OSPFHeader* ospf_header 	= (OSPFHeader*)ospf_packet;
		ospf_header->version		= 2;
		ospf_header->type 			= 1;
		ospf_header->packet_length 	= htons(ospf_len);
		ospf_header->router_id 		= htonl(myconfigs.router_id);
		ospf_header->area_id   		= htonl(interface->area->id);
		ospf_header->checksum 		= 0;
		ospf_header->autype 		= interface->au_type;
		if (interface->au_type == 0) {
			ospf_header->authentication[0] = 0;
			ospf_header->authentication[1] = 0;
		} else {
			perror("SendPacket: Unsupport Authentication Type\n");
		}

		/* 载荷 */

		OSPFHello* ospf_hello = (OSPFHello*)(ospf_packet + sizeof(OSPFHeader)); 
        ospf_hello->ip_interface_mask				= htonl(interface->ip_interface_mask);	// 对应接口的 ip_interface_mask
        ospf_hello->hello_interval				= htons(myconfigs.hello_interval);
        ospf_hello->options						= 0x02;
        ospf_hello->rtr_pri						= 1;
        ospf_hello->router_dead_interval		= htonl(myconfigs.dead_interval);
        ospf_hello->designated_router			= htonl(interface->dr);
        ospf_hello->backup_designated_router	= htonl(interface->bdr);

		/* 填写 Neighbors 的 router id */
		uint32_t* ospf_hello_neighbor = (uint32_t*)(ospf_packet + sizeof(OSPFHeader) + sizeof(OSPFHello));
        for (auto nbr: interface->neighbors) {
            *ospf_hello_neighbor++ = htonl(nbr->id);
        }

		/* 计算校验和 */
		ospf_header->checksum 		= ospf_checksum(ospf_packet, ospf_len);

		/* Send Packet */
		if (sendto(socket_fd, ospf_packet, ospf_len, 0, (struct sockaddr*)&dst_sockaddr, sizeof(dst_sockaddr)) < 0) {
			perror("SendHelloPacket: sendto");
		}
		else {
			debugf("SendHelloPacket: send success\n");
		}
        sleep(myconfigs.hello_interval);
	}
	free(ospf_packet);
}

void* recv_ospf_packet_thread(void *inter) {
	Interface *interface = (Interface*)inter;

	/* socket(AF_INET, SOCK_RAW, proto_ospf->p_proto) 无法正确接收包 */
	int socket_fd;
	if ((socket_fd = socket(AF_PACKET, SOCK_RAW, htons(ETH_P_IP))) < 0) {
        perror("[Thread]RecvPacket: socket_fd init");
    }
    // if ((socket_fd = socket(AF_INET, SOCK_RAW, proto_ospf->p_proto)) < 0) {
    //     perror("SendHelloPacket: socket_fd init");
    // }

	struct ifreq ifr;
	memset(&ifr, 0, sizeof(ifr));
	strcpy(ifr.ifr_name, myconfigs.nic_name);
    if (setsockopt(socket_fd, SOL_SOCKET, SO_BINDTODEVICE, &ifr, sizeof(ifr)) < 0) {
        perror("RecvPacket: setsockopt");
    }

	char* frame = (char*)malloc(1514);
	char* packet;
    struct iphdr *ip_header;
    struct in_addr src, dst;
	while (true) {
		memset(frame, 0, 1514);
		recv(socket_fd, frame, 1514, 0);

		packet = frame + sizeof(struct ethhdr);
		ip_header = (struct iphdr*)packet;

		/* 检查是否是 OSPF 包 */
		if (ip_header->protocol != proto_ospf->p_proto) {
			continue;
		} else {
			// debugf("RecvPacket: OSPF packet\n");
		}

		/* 检查发送的对象是否是本机 or 广播地址 */

		src.s_addr = ip_header->saddr;
		dst.s_addr = ip_header->daddr;

		if (dst.s_addr != htonl(interface->ip_interface_address) && dst.s_addr != inet_addr("224.0.0.5")) {
			continue;
		}

		/* 解析 OSPF 包 */
		OSPFHeader* ospf_header = (OSPFHeader*)(packet + sizeof(struct iphdr));
		
		debugf("RecvPacket: from %s", inet_ntoa(src));
		/* 处理 Hello 包 */
		if (ospf_header->type == T_HELLO) {
			debugf(" Hello packet\n");
			OSPFHello* ospf_hello = (OSPFHello*)(packet + sizeof(struct iphdr) + sizeof(OSPFHeader));

			/* 检查选项参数是否匹配 */
			if (ospf_hello->options != interface->ospf_options) {
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
			uint32_t pre_dr, pre_bdr;
			uint8_t pre_pri;
			bool new_neighbor = false;

			auto neighbor = interface->find_neighbor(src_ip);
			if (neighbor == nullptr) {
				Neighbor* neig = new Neighbor(src_ip);
				interface->add_neighbor(neig);
				debugf("RecvPacket: Add %s into neighbors\n", n_ip2string(ospf_header->router_id));
				neighbor = interface->find_neighbor(src_ip);
				new_neighbor = true;
				pre_pri = 0;
				pre_dr 	= 0;
				pre_bdr = 0;
			} else {
				pre_pri = neighbor->pri;
				pre_dr 	= neighbor->dr;
				pre_bdr = neighbor->bdr;
			}
			// 检查 options
			neighbor->id	= ntohl(ospf_header->router_id);
			neighbor->dr	= ntohl(ospf_hello->designated_router);
			neighbor->bdr	= ntohl(ospf_hello->backup_designated_router);
			neighbor->pri	= ospf_hello->rtr_pri;
			neighbor->options = ospf_hello->options;

			assert(neighbor != nullptr);
			/* 收到 Hello 包 */
			neighbor->event_hello_received();

			/* 检查自己的 router id 是否在 Hello 报文中 */
			uint32_t* ospf_hello_neighbor 	= (uint32_t*)(packet + sizeof(struct iphdr) + sizeof(OSPFHeader) + sizeof(OSPFHello));
			uint32_t* ospf_end 				= (uint32_t*)(packet + sizeof(struct iphdr) + ntohs(ospf_header->packet_length));
            bool b_2way = false;
			for (;ospf_hello_neighbor != ospf_end; ++ospf_hello_neighbor) {
				if (*ospf_hello_neighbor == htonl(myconfigs.router_id)) {
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

			assert(neighbor != nullptr);
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
				&& neighbor->id > myconfigs.router_id) {
					neighbor->is_master = true;
					neighbor->dd_seq_num = seq_num;
					neighbor->event_negotiation_done();
				}
				/* neighbor is slave，*/
				else if (ospf_dd->b_I == 0 && ospf_dd->b_MS == 0
				&& seq_num == neighbor->dd_seq_num
				&& neighbor->id < myconfigs.router_id) {
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
						debugf("DD packet IGNORE for receiving dumplicated packet. (S_EXSTART)\n");
					}
					continue;
				}
				/* 选项不同 */
				if (ospf_dd->options != neighbor->options) {
					neighbor->event_seq_number_mismatch();
					continue;
				}
				/* 意外设置了 I 位 */
				if (ospf_dd->b_I == 1) {
					neighbor->event_seq_number_mismatch();
					continue;
				}
				/* MS 与主从关系不匹配或者 dd_seq_num 不匹配 */
				if (neighbor->is_master) {
					if (ospf_dd->b_MS == 0 || ospf_dd->dd_seq_num != neighbor->dd_seq_num + 1) {
						neighbor->event_seq_number_mismatch();
						continue;
					}
				} else {
					if (ospf_dd->b_MS == 1 || ospf_dd->dd_seq_num != neighbor->dd_seq_num) {
						neighbor->event_seq_number_mismatch();
						continue;
					}
				}
			} else if (neighbor->state == NeighborState::S_LOADING || neighbor->state == NeighborState::S_FULL) {
				/* 只可能接收重复包 */
				if (is_dumplicated) {
					if (neighbor->is_master) {
						send_ospf_packet(neighbor->ip, T_DD, neighbor->last_send_dd_data, neighbor->last_send_dd_data_len, interface);
					} else {
						debugf("DD packet IGNORE for receiving dumplicated packet. (S_EXSTART)\n");
					}
				} else {
					debugf("DD packet IGNORE for not dumplicating packet. (S_LOADING)\n");
				}
				continue;
			} else {
				debugf("DD packet REJECT for not support state (%d)\n", neighbor->state);
				continue;
			}
			
			/* 接收到一个序号匹配的 DD 包 */
			LSAHeader* lsa_header_end	= (LSAHeader*)(packet + sizeof(iphdr) + ntohs(ospf_header->packet_length));
			/* 检查 lsa_type */
			for (LSAHeader* header = (LSAHeader*)(packet + sizeof(iphdr) + sizeof(OSPFHeader) + sizeof(OSPFDD));
			header != lsa_header_end; ++header) {
				if (header->ls_type < 1 || header->ls_type > 5) {
					debugf("DD packet REJECT for invalid LSA Header\n");
					neighbor->event_seq_number_mismatch();
					continue;
				}
				/* TODO ls_type == 5 && 邻居关联到存根区域 */
				// if (header->ls_type == 5)
			}
			/* 对比 dd 和 lsdb 中的 LSA Header */
			LSDB* lsdb = &interface->area->lsdb;
			for (LSAHeader* header = (LSAHeader*)(packet + sizeof(iphdr) + sizeof(OSPFHeader) + sizeof(OSPFDD));
			header != lsa_header_end; ++header) {
				LSAHeader lsa_header;
				lsa_header.ls_age 				= ntohs(header->ls_age);
				lsa_header.options 				= header->options;
				lsa_header.ls_type 				= header->ls_type;
				lsa_header.link_state_id 		= ntohl(header->link_state_id);
				lsa_header.advertising_router 	= ntohl(header->advertising_router);
				lsa_header.ls_seq_num 			= ntohl(header->ls_seq_num);
				lsa_header.ls_checksum 			= ntohs(header->ls_checksum);
				lsa_header.length 				= ntohs(header->length);
				if (lsa_header.ls_type == LSA_ROUTER) {
					LSARouter* lsa_router = lsdb->find_router_lsa(lsa_header.link_state_id, lsa_header.advertising_router);
					if (lsa_router == nullptr) {
						neighbor->link_state_request_list.push_back(lsa_header);
					} else {
						if (lsa_header_cmp(&lsa_header, &lsa_router->header) < 0) {
							neighbor->link_state_request_list.push_back(lsa_header);
						}
					}
				} else if (lsa_header.ls_age == LSA_NETWORK) {
					LSANetwork* lsa_network = lsdb->find_network_lsa(lsa_header.link_state_id, lsa_header.advertising_router);
					if (lsa_network == nullptr) {
						neighbor->link_state_request_list.push_back(lsa_header);
					} else {
						if (lsa_header_cmp(&lsa_header, &lsa_network->header) < 0) {
							neighbor->link_state_request_list.push_back(lsa_header);
						}
					}
				} else {
					debugf("DD packet REJECT for invalid LSA Header\n");
					continue;
				}
			}

			/* 发送一个 DD 包 */
			char* send_dd_data = (char*)malloc(1024);
			uint32_t send_dd_data_len = sizeof(OSPFDD);

			OSPFDD* send_ospf_dd = (OSPFDD*)send_dd_data;
			memset(send_ospf_dd, 0, sizeof(OSPFDD));
			
			send_ospf_dd->interface_mtu = htons(neighbor->interface->mtu);
			send_ospf_dd->options = 0x02;
			send_ospf_dd->b_I = 0;
			LSAHeader* header = (LSAHeader*)(send_dd_data + sizeof(OSPFDD));

			if (neighbor->is_master) {
				send_ospf_dd->b_MS 			= 0;
				send_ospf_dd->dd_seq_num 	= htons(neighbor->dd_seq_num);
				if (neighbor->last_send_dd_data_len != 0) {
					int cnt = (neighbor->last_send_dd_data_len - sizeof(OSPFDD)) / sizeof(LSAHeader);
					while (cnt) {
						neighbor->database_summary_list.pop_front();
						--cnt;
					}
				}
				send_ospf_dd->b_M			= !neighbor->database_summary_list.empty();
				for (auto lsa_header: neighbor->database_summary_list) {
					if (send_dd_data_len + sizeof(LSAHeader) > 1024) {
						break;
					}
					header->ls_age 				= htons(lsa_header.ls_age);
					header->options 			= lsa_header.options;
					header->ls_type 			= lsa_header.ls_type;
					header->link_state_id 		= htonl(lsa_header.link_state_id);
					header->advertising_router 	= htonl(lsa_header.advertising_router);
					header->ls_seq_num 			= htonl(lsa_header.ls_seq_num);
					header->ls_checksum 		= htons(lsa_header.ls_checksum);
					header->length 				= htons(lsa_header.length);
					++header;
					send_dd_data_len 			+= sizeof(LSAHeader);
				}
				send_ospf_packet(neighbor->ip, T_DD, send_dd_data, send_dd_data_len, interface);
				memcpy(neighbor->last_send_dd_data, send_dd_data, send_dd_data_len);
				neighbor->last_send_dd_data_len = send_dd_data_len;
				if (ospf_dd->b_M == 0 && send_ospf_dd->b_M == 0) {
					neighbor->event_exchange_done();
				}
			} else {
				neighbor->dd_seq_num++;
				send_ospf_dd->b_MS = 1;
				send_ospf_dd->dd_seq_num = neighbor->dd_seq_num;
				if (neighbor->last_send_dd_data_len != 0) {
					int cnt = (neighbor->last_send_dd_data_len - sizeof(OSPFDD)) / sizeof(LSAHeader);
					while (cnt) {
						neighbor->database_summary_list.pop_front();
						--cnt;
					}
				}
				send_ospf_dd->b_M			= !neighbor->database_summary_list.empty();
				for (auto lsa_header: neighbor->database_summary_list) {
					if (send_dd_data_len + sizeof(LSAHeader) > 1024) {
						break;
					}
					header->ls_age 				= htons(lsa_header.ls_age);
					header->options 			= lsa_header.options;
					header->ls_type 			= lsa_header.ls_type;
					header->link_state_id 		= htonl(lsa_header.link_state_id);
					header->advertising_router 	= htonl(lsa_header.advertising_router);
					header->ls_seq_num 			= htonl(lsa_header.ls_seq_num);
					header->ls_checksum 		= htons(lsa_header.ls_checksum);
					header->length 				= htons(lsa_header.length);
					++header;
					send_dd_data_len 			+= sizeof(LSAHeader);
				}
				debugf("neibor M: %d, interface M: %d\n", ospf_dd->b_M, send_ospf_dd->b_M);
				if (ospf_dd->b_M == 0 && send_ospf_dd->b_M == 0) {
					neighbor->event_exchange_done();
				} else {
					send_ospf_packet(neighbor->ip, T_DD, send_dd_data, send_dd_data_len, interface);
					memcpy(neighbor->last_send_dd_data, send_dd_data, send_dd_data_len);
					neighbor->last_send_dd_data_len = send_dd_data_len;
				}
			}
			free(send_dd_data);
		} else {
			debugf(" <TODO> packet\n");
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
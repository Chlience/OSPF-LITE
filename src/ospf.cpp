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

extern GlobalConfig myconfigs;

struct protoent *proto_ospf;

int ospf_init() {
	proto_ospf = getprotobyname("ospf");
	if (proto_ospf == NULL) {
		perror("getprotobyname() failed");
		return -1;
	}
	else {
		printf("proto_ospf->p_proto: %d\n", proto_ospf->p_proto);
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
	const uint8_t ospf_type, const char* ospf_data, const size_t ospf_data_len) {

	int socket_fd;
    if ((socket_fd = socket(AF_INET, SOCK_RAW, proto_ospf->p_proto)) < 0) {
        perror("SendPacket: socket_fd init");
    }

	/* 将 socket 绑定到特定到网卡，所有的包都将通过该网卡进行发送 */
	/* 在 linux 中，所有网络设备都使用 ifreq 来标识 */
	/* 在此处只需要使用 ifreq 的 ifr_name */
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
    ospf_header->area_id   		= htonl(0 /* TODO */);
    ospf_header->checksum 		= 0;
    ospf_header->autype 		= 0;
    ospf_header->authentication[0] = 0;
    ospf_header->authentication[1] = 0;

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
		ospf_header->area_id   		= htonl(myconfigs.area->area_id);
		ospf_header->checksum 		= 0;
		ospf_header->autype 		= 0;
		ospf_header->authentication[0] = 0;
		ospf_header->authentication[1] = 0;

		/* 载荷 */

		OSPFHello* ospf_hello = (OSPFHello*)(ospf_packet + sizeof(OSPFHeader)); 
        ospf_hello->network_mask				= htonl(interface->network_mask);	// 对应接口的 network_mask
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
			printf("SendHelloPacket: send success\n");
		}
        sleep(myconfigs.hello_interval);
	}
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
		size_t recv_size = recv(socket_fd, frame, 1514, 0);

		packet = frame + sizeof(struct ethhdr);
		ip_header = (struct iphdr*)packet;

		/* 检查是否是 OSPF 包 */
		if (ip_header->protocol == proto_ospf->p_proto) {
			debugf("RecvPacket: OSPF packet, size %lu\n", recv_size);
		}
		else {
			// debugf("RecvPacket: Not OSPF packet\n");
			continue;
		}

		/* 检查发送的对象是否是本机 or 广播地址 */

		src.s_addr = ip_header->saddr;
		dst.s_addr = ip_header->daddr;

		if (dst.s_addr != htonl(interface->ip) && dst.s_addr != inet_addr("224.0.0.5")) {
			debugf("RecvPacket: Not for me\n");
			continue;
		} else {
			debugf("RecvPacket: From %s\n", inet_ntoa(src));
			debugf("RecvPacket: TO   %s\n", inet_ntoa(dst));
		}

		/* 解析 OSPF 包 */
		OSPFHeader* ospf_header = (OSPFHeader*)(packet + sizeof(struct iphdr));
		
		printf("RecvPacket: RID %s", n_ip2string(ospf_header->router_id));
		/* 处理 Hello 包 */
		if (ospf_header->type == T_HELLO) {
			debugf(" Hello packet\n");
			OSPFHello* ospf_hello = (OSPFHello*)(packet + sizeof(struct iphdr) + sizeof(OSPFHeader));

			// 将 src 加入到邻居中

			uint32_t src_ip = ntohl(src.s_addr);
			uint32_t dst_ip = ntohl(dst.s_addr);
			uint32_t pre_pri, pre_dr, pre_bdr;
			bool new2way = false;

			auto neighbor = interface->find_neighbor(src_ip);
			if (neighbor == nullptr) {
				Neighbor* neig = new Neighbor(src_ip);
				interface->add_neighbor(neig);
				debugf("RecvPacket: Add %s into neighbors\n", n_ip2string(ospf_header->router_id));
				neighbor = interface->find_neighbor(src_ip);
				/* 设置 pre DR 和 BDR 和当前一致，不触发事件 */
				new2way = true;
				pre_pri = ntohl(ospf_hello->rtr_pri);
				pre_dr 	= ntohl(ospf_hello->designated_router);	
				pre_bdr = ntohl(ospf_hello->backup_designated_router);
			} else {
				pre_pri = neighbor->pri;
				pre_dr 	= neighbor->dr;
				pre_bdr = neighbor->bdr;
			}
			neighbor->id	= ntohl(ospf_header->router_id);
			neighbor->dr	= ntohl(ospf_hello->designated_router);
			neighbor->bdr	= ntohl(ospf_hello->backup_designated_router);
			neighbor->pri	= ntohl(ospf_hello->rtr_pri);

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

			if (new2way) {
				interface->event_neighbor_change();
			} else if (pre_pri != neighbor->pri) {
				interface->event_neighbor_change();
			} else if (neighbor->dr == neighbor->ip && neighbor->bdr == 0x00000000 && interface->state == InterfaceState::S_WAITING) {
				interface->event_backup_seen();
			} else if (pre_dr != neighbor->ip && neighbor->dr == neighbor->ip) {
				interface->event_neighbor_change();
			} else if (pre_dr == neighbor->ip && neighbor->dr != neighbor->ip) {
				interface->event_neighbor_change();
			} else if (neighbor->bdr == neighbor->ip && interface->state == InterfaceState::S_WAITING) {
				interface->event_backup_seen();
			} else if (pre_bdr != neighbor->ip && neighbor->bdr == neighbor->ip) {
				interface->event_neighbor_change();
			} else if (pre_bdr == neighbor->ip && neighbor->bdr != neighbor->ip) {
				interface->event_neighbor_change();
			} 
		}
	}
}
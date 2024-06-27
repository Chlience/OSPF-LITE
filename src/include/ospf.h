#ifndef OSPF_H
#define OSPF_H

#include <cstdint>
#include <stddef.h>

#include "interface.h"

struct OSPFHeader {
    uint8_t     version;
    uint8_t     type;
    uint16_t    packet_length;
    uint32_t    router_id;
    uint32_t    area_id;
    uint16_t    checksum;
    uint16_t    autype;
    uint32_t    authentication[2];
};	// 24 bytes

struct OSPFHello {
    uint32_t    ip_interface_mask;
    uint16_t    hello_interval;
    uint8_t     options;
    uint8_t     rtr_pri;
    uint32_t    router_dead_interval;
    uint32_t    designated_router;
    uint32_t    backup_designated_router;
};  // 20 bytes

struct OSPFDD {
    uint16_t    interface_mtu;
    uint8_t     options;
    uint8_t     b_MS: 1;
    uint8_t     b_M : 1;
    uint8_t     b_I : 1;
    uint8_t     b_other: 5;
    uint32_t    dd_seq_num;
};  // 8 bytes

struct OSPFLsr {
    uint32_t    ls_type;
    uint32_t    link_state_id;
    uint32_t    advertising_router;
};  // 12 bytes

struct OSPFLsu {
    uint32_t    num;
};

enum OSPFType: uint8_t {
    T_HELLO = 1,
    T_DD,
    T_LSR,
    T_LSU,
    T_LSAck
};

int ospf_init();
void send_ospf_packet(uint32_t dst_ip, const uint8_t ospf_type, const char* ospf_data, const size_t ospf_data_len);
void* send_ospf_hello_packet_thread(void* interface);
void* recv_ospf_packet_thread(void* interface);
void* send_empty_dd_packet_thread(void* neighbor);
void* send_lsr_packet_thread(void* neighbor);
int flooding_lsa(void* lsa_header_ptr, Interface* interface, bool is_origin, in_addr src);
#endif
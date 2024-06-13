#ifndef OSPF_H
#define OSPF_H

#include <cstdint>
#include <stddef.h>

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
    uint32_t    network_mask;
    uint16_t    hello_interval;
    uint8_t     options;
    uint8_t     rtr_pri;
    uint32_t    router_dead_interval;
    uint32_t    designated_router;
    uint32_t    backup_designated_router;
};  // 20 bytes

int ospf_init();
void send_ospf_packet(uint32_t dst_ip, const uint8_t ospf_type, const char* ospf_data, const size_t ospf_data_len);
void* send_ospf_hello_package_thread(void* interface);

#endif
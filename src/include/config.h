#ifndef CONFIG_H
#define CONFIG_H

#include <cstdint>
#include <stddef.h>
#include <cstring>

struct GlobalConfig {
    const char* nic_name;
    uint32_t    ip;
    uint32_t    ip_interface_mask;
    
    uint32_t    router_id;
	uint32_t	hello_interval  = 10;
	uint32_t	dead_interval   = 40;
	uint32_t	wait_interval   = 40;
    uint8_t     ospf_options    = 0x2; // OPTION_E
    // uint32_t    inf_trans_delay = 1;
    uint32_t    ls_sequence_cnt;
    // Constructor
    GlobalConfig() {
    }

    ~GlobalConfig() {}
};

uint32_t get_ip_address(const char*);
uint32_t get_network_mask(const char*);

#endif
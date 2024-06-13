#ifndef CONFIG_H
#define CONFIG_H

#include <cstdint>
#include <stddef.h>
#include <cstring>

#include "ospf_interface.h"

struct GlobalConfig {
    const char* nic_name;
    uint32_t    ip;
    uint32_t    network_mask;
    
    uint32_t    router_id;
	uint32_t	hello_interval = 10;
	uint32_t	dead_interval = 40;
	uint32_t	wait_interval = 40;

    OSPFArea*   area;

    // Constructor
    GlobalConfig() {
    }

    ~GlobalConfig() {}
};

uint32_t get_ip_address(const char*);
uint32_t get_network_mask(const char*);

#endif
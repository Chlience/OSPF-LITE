#ifndef CONFIG_H
#define CONFIG_H

#include <cstdint>
#include <stddef.h>
#include <cstring>

struct GlobalConfig {
    const char* nic_name;
    uint32_t router_id;

    // Constructor
    GlobalConfig(const char* name, uint32_t id) {
        // Allocate memory and copy the nic_name
        nic_name = new char[strlen(name) + 1];
        strcpy(const_cast<char*>(nic_name), name);
        
        // Assign router_id
        router_id = id;
    }

    // Destructor to free allocated memory
    ~GlobalConfig() {
        delete[] nic_name;
    }
};

#endif
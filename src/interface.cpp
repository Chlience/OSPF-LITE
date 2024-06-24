#include <assert.h>
#include "interface.h"
#include "neighbor.h"
#include "debug.h"
#include "config.h"

extern GlobalConfig myconfigs;

Neighbor* Interface::find_neighbor(uint32_t ip) {
    for (auto iter : neighbors) {
        // debugf("find_neighbor: %s\n", ip2string(iter->ip));
        if (iter->ip == ip) {
            return iter;
        }
    }
    return nullptr;
}

void Interface::add_neighbor(Neighbor* neighbor) {
    neighbor->interface = this;
    neighbors.push_back(neighbor);
}

void Interface::event_interface_up() {
    printf("Interface %s event_interface_up", ip2string(ip_interface_address));
    if (state == InterfaceState::S_DOWN) {
        if (type == NetworkType::T_P2P || type == NetworkType::T_P2MP || type == NetworkType::T_VIRTUAL) {
            printf(" from DOWN to POINT2POINT\n");
            state = InterfaceState::S_POINT2POINT;
        } else {
            if (router_priority != 0) {
                printf(" from DOWN to WAITING\n");
                state = InterfaceState::S_WAITING;
            } else {
                printf(" from DOWN to DROTHER\n");
                state = InterfaceState::S_DROTHER;
            }
        }
    }
}

void Interface::event_wait_timer() {
    /* TODO */
}

void Interface::event_backup_seen() {
    if (state == InterfaceState::S_WAITING) {
        call_election();
        if (ip_interface_address == dr) {
            printf("Interface %s event_backup_seen", ip2string(ip_interface_address));
            printf(" from WAITING to DR\n");
            state = InterfaceState::S_DR;
        } else if (ip_interface_address == bdr) {
            printf("Interface %s event_backup_seen", ip2string(ip_interface_address));
            printf(" from WAITING to BACKUP\n");
            state = InterfaceState::S_BACKUP;
        }
        else {
            printf("Interface %s event_backup_seen", ip2string(ip_interface_address));
            printf(" from WAITING to DROTHER\n");
            state = InterfaceState::S_DROTHER;
        }
    } else {
        printf("Interface %s event_backup_seen", ip2string(ip_interface_address));
        printf(" REJCET\n");
    }
}

void Interface::event_neighbor_change() {
    // printf("Interface %s event_neighbor_change", ip2string(ip_interface_address));
    if (state == InterfaceState::S_DR
        || state == InterfaceState::S_BACKUP
        || state == InterfaceState::S_DROTHER) {
        call_election();
        if (ip_interface_address == dr) {
            printf("Interface %s event_neighbor_change", ip2string(ip_interface_address));
            printf(" from DR/BACKUP/DROTHER to DR\n");
            state = InterfaceState::S_DR;
        } else if (ip_interface_address == bdr) {
            printf("Interface %s event_neighbor_change", ip2string(ip_interface_address));
            printf(" from DR/BACKUP/DROTHER to BACKUP\n");
            state = InterfaceState::S_BACKUP;
        }
        else {
            printf("Interface %s event_neighbor_change", ip2string(ip_interface_address));
            printf(" from DR/BACKUP/DROTHER to DROTHER\n");
            state = InterfaceState::S_DROTHER;
        }
    } else {
        printf("Interface %s event_neighbor_change", ip2string(ip_interface_address));
        printf(" REJCET\n");
    }
}

void Interface::event_loop_ind() {

}

void Interface::event_unloop_ind() {

}

void Interface::event_interface_down() {

}

void election_for_dr_and_ndr(Interface* interface, Neighbor** dr_ptr, Neighbor** bdr_ptr, bool no_bdr) {
    std::list<Neighbor*> candidates;

    /* 不加入 BDR 选举 */
    if (no_bdr == false && interface->router_priority != 0) {
        Neighbor self(interface->ip_interface_address);
        self.id     = myconfigs.router_id;
        self.dr     = interface->dr;
        self.bdr    = interface->bdr;
        self.pri    = interface->router_priority;
        candidates.push_back(&self);
    }

    for (auto neighbor: interface->neighbors) {
        if (neighbor->state >= NeighborState::S_2WAY && neighbor->pri != 0) {
            candidates.push_back(neighbor);
        }
    }
    
    Neighbor* dr  = nullptr;
    Neighbor* bdr = nullptr;

    /* BDR */
    for (auto candidate: candidates) {
        if (candidate->dr != candidate->ip) {
            if (bdr == nullptr) {
                bdr = candidate;
            } else {
                /* 如果当前 bdr 参与选举 */
                if (bdr->bdr == bdr->ip) {
                    /* 参选人必须参与选举 */
                    if (candidate->bdr == candidate->ip && (candidate->pri > bdr->pri || (candidate->pri == bdr->pri && candidate->id > bdr->id))) {
                        bdr = candidate;
                    }
                /* 如果当前 bdr 不参与选举 */
                } else {
                    /* 参选人可以不参与选举 */
                    if (candidate->bdr == candidate->ip || (candidate->pri > bdr->pri || (candidate->pri == bdr->pri && candidate->id > bdr->id))) {
                        bdr = candidate;
                    }
                }
            }
        }
    }
    assert(bdr != nullptr);

    /* 重新加入 DR 选举 */
    if (no_bdr == true && interface->router_priority != 0) {
        Neighbor self(interface->ip_interface_address);
        self.id     = myconfigs.router_id;
        self.dr     = interface->dr;
        self.bdr    = interface->bdr;
        self.pri    = interface->router_priority;
        candidates.push_back(&self);
    }

    /* DR */
    for (auto candidate: candidates) {
        if (candidate->dr != candidate->ip) {
            continue;
        }
        if (dr == nullptr) {
            dr = candidate;
        } else {
            if (candidate->pri > dr->pri || (candidate->pri == dr->pri && candidate->id > dr->id)) {
                dr = candidate;
            }
        }
    }

    if (dr == nullptr) {
        dr = bdr;
    }

    *dr_ptr  = dr;
    *bdr_ptr = bdr;
}

void Interface::call_election() {
    Neighbor* dr  = nullptr;
    Neighbor* bdr = nullptr;

    uint32_t old_old_dr_ip  = this->dr;
    uint32_t old_old_bdr_ip = this->bdr;
    uint32_t old_dr_ip;
    uint32_t old_bdr_ip;
    bool no_bdr = false;
    while(1) {
        election_for_dr_and_ndr(this, &dr, &bdr, no_bdr);
        old_dr_ip   = this->dr;
        old_bdr_ip  = this->bdr;
        this->dr    = dr->ip;
        this->bdr   = bdr->ip;
        if (ip_interface_address == dr->ip && ip_interface_address != old_dr_ip) {
            no_bdr = true;
        } else if (ip_interface_address == bdr->ip && ip_interface_address != old_bdr_ip) {
            no_bdr = false;
        } else {
            break;
        }
    }
    if (old_old_dr_ip != this->dr || old_old_bdr_ip != this->bdr) {
        for (auto neighbor: neighbors) {
            neighbor->event_adj_ok();
        }
    }
}
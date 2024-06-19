#include "interface.h"
#include "neighbor.h"

Neighbor* Interface::find_neighbor(uint32_t ip) {
    for (auto iter : neighbors) {
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
    if (state == InterfaceState::S_DOWN) {
        if (type == NetworkType::T_P2P || type == NetworkType::T_P2MP || type == NetworkType::T_VIRTUAL) {
            state = InterfaceState::S_POINT2POINT;
        } else {
            if (can_be_dr) {
                state = InterfaceState::S_WAITING;
            } else {
                state = InterfaceState::S_DROTHER;
            }
        }
    }
}

void Interface::event_wait_timer() {

}

void Interface::event_backup_seen() {

}

void Interface::event_neighbor_change() {

}

void Interface::event_loop_ind() {

}

void Interface::event_unloop_ind() {

}

void Interface::event_interface_down() {

}
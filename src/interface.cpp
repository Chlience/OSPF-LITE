#include "interface.h"
#include "neighbor.h"

std::set<Neighbor>::iterator Interface::find_neighbor(uint32_t ip) {
    Neighbor temp_neighbor(ip);
    return neighbors.find(temp_neighbor);
}

void Interface::add_neighbor(Neighbor neighbor) {
    neighbors.insert(neighbor);
}
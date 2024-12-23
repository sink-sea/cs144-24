#include "router.hh"
#include "address.hh"
#include "ipv4_datagram.hh"

#include <iostream>
#include <optional>
#include <queue>

using namespace std;

// route_prefix: The "up-to-32-bit" IPv4 address prefix to match the datagram's destination address against
// prefix_length: For this route to be applicable, how many high-order (most-significant) bits of
//    the route_prefix will need to match the corresponding bits of the datagram's destination address?
// next_hop: The IP address of the next hop. Will be empty if the network is directly attached to the router (in
//    which case, the next hop address should be the datagram's final destination).
// interface_num: The index of the interface to send the datagram out on.
void Router::add_route(const uint32_t route_prefix,
                       const uint8_t prefix_length,
                       const optional<Address> next_hop,
                       const size_t interface_num) {
    cerr << "DEBUG: adding route " << Address::from_ipv4_numeric(route_prefix).ip() << "/"
         << static_cast<int>(prefix_length) << " => " << (next_hop.has_value() ? next_hop->ip() : "(direct)")
         << " on interface " << interface_num << " with name " << _interfaces[interface_num]->name() << "\n";

    forwarding_table_.push_back({route_prefix, prefix_length, interface_num, next_hop});
}

// Go through all the interfaces, and route every incoming datagram to its proper outgoing interface.
void Router::route() {
    /* datagrams to send to corresponding interfaces */
    queue<tuple<uint64_t, InternetDatagram, optional<Address>>> route_dgrams_ {};

    for (auto& interface : _interfaces) {
        auto& datagrams = interface->datagrams_received();
        while (not datagrams.empty()) {
            auto dgram_ = datagrams.front();
            datagrams.pop();
            if (dgram_.header.ttl == 0) {
                continue;
            }
            /* update TTL and checksum */
            dgram_.header.ttl--;
            dgram_.header.compute_checksum();

            /* longest prefix matching */
            uint8_t max_prefix_length {};
            uint32_t next_hop_interface {};
            optional<Address> next_hop_addr {};
            for (auto& [router_prefix_, prefix_length_, num_, addr_] : forwarding_table_) {
                if (max_prefix_length > prefix_length_) {
                    continue;
                }
                if ((dgram_.header.dst ^ router_prefix_) < (1ul << (32 - prefix_length_))) {
                    next_hop_interface = num_;
                    next_hop_addr = addr_;
                    max_prefix_length = prefix_length_;
                }
            }
            route_dgrams_.push({next_hop_interface, move(dgram_), next_hop_addr});
        }
    }

    while (not route_dgrams_.empty()) {
        auto [interface_num_, dgram_, next_hop_addr_] = route_dgrams_.front();
        route_dgrams_.pop();
        _interfaces[interface_num_]->send_datagram(
          dgram_, next_hop_addr_.value_or(Address::from_ipv4_numeric(dgram_.header.dst)));
    }
}

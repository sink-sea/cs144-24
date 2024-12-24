#include <iostream>

#include "arp_message.hh"
#include "exception.hh"
#include "network_interface.hh"

using namespace std;

//! \param[in] ethernet_address Ethernet (what ARP calls "hardware") address of the interface
//! \param[in] ip_address IP (what ARP calls "protocol") address of the interface
NetworkInterface::NetworkInterface(string_view name,
                                   shared_ptr<OutputPort> port,
                                   const EthernetAddress& ethernet_address,
                                   const Address& ip_address)
  : name_(name)
  , port_(notnull("OutputPort", move(port)))
  , ethernet_address_(ethernet_address)
  , ip_address_(ip_address) {
    cerr << "DEBUG: Network interface has Ethernet address " << to_string(ethernet_address) << " and IP address "
         << ip_address.ip() << " and name " << name << "\n";
}

//! \param[in] dgram the IPv4 datagram to be sent
//! \param[in] next_hop the IP address of the interface to send it to (typically a router or default gateway, but
//! may also be another host if directly connected to the same network as the destination) Note: the Address type
//! can be converted to a uint32_t (raw 32-bit IP address) by using the Address::ipv4_numeric() method.
void NetworkInterface::send_datagram(const InternetDatagram& dgram, const Address& next_hop) {
    if (dgram.header.ttl == 0) {
        return;
    }

    auto next_hop_ip = next_hop.ipv4_numeric();
    EthernetFrame ipv4_frame {{{}, ethernet_address_, EthernetHeader::TYPE_IPv4}};

    Serializer serial_ {};
    dgram.serialize(serial_);
    ipv4_frame.payload = serial_.output();

    /* ARP table hit */
    if (arp_table_.contains(next_hop_ip)) {
        ipv4_frame.header.dst = arp_table_[next_hop_ip].first;
        transmit(ipv4_frame);
        return;
    }

    /* store unsent frames, and wait for ARP reply */
    frames_to_send_[next_hop_ip].push_back(ipv4_frame);

    /* ARP table miss, and not requested yet */
    if (not arp_requests_sent_.contains(next_hop_ip)) {
        /* send an ARP request to get next_hop's MAC address */
        auto arp_request {make_arp_message(ARPMessage::OPCODE_REQUEST, next_hop_ip, {})};
        EthernetHeader header_ {ETHERNET_BROADCAST, ethernet_address_, EthernetHeader::TYPE_ARP};

        auto arp_request_frame {wrap_arp_message(arp_request, header_)};
        arp_requests_sent_[next_hop_ip] = {arp_request_frame, DEFAULT_ARP_RTO_};
        transmit(arp_request_frame);
    }
}

//! \param[in] frame the incoming Ethernet frame
void NetworkInterface::recv_frame(const EthernetFrame& frame) {
    if (frame.header.dst != ethernet_address_ and frame.header.dst != ETHERNET_BROADCAST) {
        return;
    }

    Parser parser_ {frame.payload};

    /* IPv4 frames: parse and deliver */
    if (frame.header.type == EthernetHeader::TYPE_IPv4) {
        InternetDatagram ipv4_datagram {};
        ipv4_datagram.parse(parser_);

        if (parser_.has_error()) {
            return;
        }

        datagrams_received_.push(ipv4_datagram);
    }

    /* ARP frames: reply or request */
    if (frame.header.type == EthernetHeader::TYPE_ARP) {
        ARPMessage arp_msg {};
        arp_msg.parse(parser_);

        if (parser_.has_error()) {
            return;
        }

        auto sender_ip = arp_msg.sender_ip_address;
        /* get as many MAC addresses as possible */
        arp_table_[sender_ip] = {arp_msg.sender_ethernet_address, DEFAULT_ARP_TTL_};

        if (arp_msg.target_ip_address != ip_address_.ipv4_numeric()) {
            return;
        }

        /* ARP reply: send corresponding frames stored */
        if (arp_msg.opcode == ARPMessage::OPCODE_REPLY) {
            arp_requests_sent_.erase(sender_ip);
            for (auto& frame_ : frames_to_send_[sender_ip]) {
                frame_.header.dst = arp_table_[sender_ip].first;
                transmit(frame_);
            }
            frames_to_send_.erase(sender_ip);
        }

        /* ARP request: reply it */
        if (arp_msg.opcode == ARPMessage::OPCODE_REQUEST) {
            auto arp_reply {make_arp_message(ARPMessage::OPCODE_REPLY, sender_ip, arp_msg.sender_ethernet_address)};
            EthernetHeader header_ {frame.header.src, ethernet_address_, EthernetHeader::TYPE_ARP};

            transmit(wrap_arp_message(arp_reply, header_));
        }
    }
}

//! \param[in] ms_since_last_tick the number of milliseconds since the last call to this method
void NetworkInterface::tick(const size_t ms_since_last_tick) {
    /* traverse mapping tables and update */
    auto tick_and_update_ = [ms_since_last_tick](auto& map_) {
        vector<uint32_t> ip_to_erase_ {};
        for (auto& [ip_, pair_] : map_) {
            auto& TTL_ = pair_.second;
            if (TTL_ < ms_since_last_tick) {
                ip_to_erase_.emplace_back(ip_);
            }
            TTL_ -= ms_since_last_tick;
        }
        return ip_to_erase_;
    };

    for (auto& ip_ : tick_and_update_(arp_table_)) {
        arp_table_.erase(ip_);
    }

    for (auto& ip_ : tick_and_update_(arp_requests_sent_)) {
        transmit(arp_requests_sent_[ip_].first);
        arp_requests_sent_[ip_].second = DEFAULT_ARP_RTO_;
    }
}

EthernetFrame NetworkInterface::wrap_arp_message(const ARPMessage& arp_msg, const EthernetHeader& header) {
    EthernetFrame arp_reply {std::move(header)};

    Serializer serial_ {};
    arp_msg.serialize(serial_);
    arp_reply.payload = serial_.output();

    return arp_reply;
}

ARPMessage NetworkInterface::make_arp_message(const uint16_t opcode,
                                              const uint32_t target_ip_address,
                                              const EthernetAddress& target_ethernet_address) {
    ARPMessage arp_msg {};
    arp_msg.opcode = opcode;
    arp_msg.sender_ip_address = ip_address_.ipv4_numeric();
    arp_msg.sender_ethernet_address = ethernet_address_;
    arp_msg.target_ip_address = target_ip_address;
    arp_msg.target_ethernet_address = target_ethernet_address;
    return arp_msg;
}
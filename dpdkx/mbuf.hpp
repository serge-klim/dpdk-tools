#pragma once

#include "rte_byteorder.h"
#include <cstddef>
#include <cstdint>

struct rte_ether_hdr;
struct rte_ipv4_hdr;
struct rte_udp_hdr;
struct rte_mbuf;

namespace dpdkx {

rte_ether_hdr* make_ether(rte_mbuf* buffer, rte_be16_t ether_type, std::size_t payload_size);
rte_ipv4_hdr* make_ipv4(rte_mbuf* buffer, std::uint8_t next_proto_id, std::size_t payload_size);
rte_udp_hdr* make_udp(rte_mbuf* buffer, std::size_t payload_size);

} // namespace dpdkx

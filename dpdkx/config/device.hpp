#pragma once
#include "rte_ether.h"
#include "rte_ethdev.h"
#include <bitset>
#include <limits>
#include <string>
#include <cstdint>
#include <cstddef>


namespace dpdkx { inline namespace v0 { 

using port_id_t = std::uint16_t;
using ip_port_t = rte_be16_t;

namespace config {

struct device {
    std::string      id;
    unsigned int     socket_id;
    port_id_t        port_id;
    rte_ether_addr   mac_addr = {0};
    rte_be32_t       ip4_addr = 0;
    std::uint8_t     ip6_addr[16] = {0};
    std::uint16_t    svc_tx_n = (std::numeric_limits<std::uint16_t>::max)();
    std::uint16_t    next_src_port = { 49152 };
    std::uint16_t    rx_reconfig_hint = 256/*(std::numeric_limits<std::uint16_t>::max)()*/;
    //    bool             clock_swap = false;
    std::uint64_t    clock_hz = 0;
    rte_eth_dev_info info;
    struct effective_offloads {
        std::uint64_t rx = (std::numeric_limits<std::uint64_t>::max)();
        std::uint64_t tx = (std::numeric_limits<std::uint64_t>::max)();
    } effective_offload;
    static constexpr std::size_t icmp = 0;
    static constexpr std::size_t arp_table = 1;
    static constexpr std::size_t dev_clock = 2;
    std::uint64_t rx_meta_features = RTE_ETH_RX_METADATA_USER_FLAG | RTE_ETH_RX_METADATA_USER_MARK | RTE_ETH_RX_METADATA_TUNNEL_ID;
    std::bitset<64>  features = { /*(1 << icmp)|*/ (1<<arp_table) | (1<<dev_clock)};
};

}}} // namespace dpdkx::v0::config


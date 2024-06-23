#pragma once
#include "rte_byteorder.h"
#include "rte_ether.h"
#include <iterator>
#include <type_traits>
#include <cstring>
#include <cstdint>
#include <cassert>

namespace dpdkx {

template<typename T>
concept Swapable = std::is_trivially_copyable_v<T>;

template<Swapable T>
void swap_ptr_values(T* left, T* right) {
    T tmp;
    std::memcpy(&tmp, left, sizeof(tmp));
    std::memcpy(left, right, sizeof(*left));
    std::memcpy(right, &tmp, sizeof(*right));
}

inline
#ifdef NDEBUG
constexpr 
#endif
rte_be16_t eth_proto(struct rte_mbuf const* buffer) noexcept {
    assert(buffer->l2_len != 0 && "l2 len must be set!");
    assert(buffer->l2_len > sizeof(rte_be16_t));
    return *rte_pktmbuf_mtod_offset(buffer, rte_be16_t*, buffer->l2_len - sizeof(rte_be16_t));
}

inline rte_be16_t update_l2size(struct rte_mbuf* buffer) noexcept {
    auto ether_hdr = rte_pktmbuf_mtod(buffer, struct rte_ether_hdr const*);
    if ((buffer->l2_len = sizeof(*ether_hdr)) > buffer->data_len)
        return 0;
    assert(ether_hdr != nullptr);
    auto proto = ether_hdr->ether_type;
    while (proto == RTE_BE16(RTE_ETHER_TYPE_VLAN) || proto == RTE_BE16(RTE_ETHER_TYPE_QINQ)) {
        auto vlan_hdr = rte_pktmbuf_mtod_offset(buffer, struct rte_vlan_hdr const*, buffer->l2_len);
        buffer->l2_len += sizeof(*vlan_hdr);
        if (buffer->l2_len > buffer->data_len)
            return 0;
        proto = vlan_hdr->eth_proto;
    }
    assert(eth_proto(buffer) == proto);
    return proto;
}

inline rte_ether_addr ether_addr_for_ipv4_mcast(rte_be32_t addr) noexcept {
    auto res = rte_ether_addr{};
    auto int_addr = (rte_cpu_to_be_64(0x01005e000000ULL | (/*rte_cpu_to_be_32*/(addr) & 0x7fffff)) >> 16);
    static_assert(sizeof(int_addr) > sizeof(res.addr_bytes), "oops! not going to work");
    std::memcpy(&res.addr_bytes, &int_addr, sizeof(res.addr_bytes));
    return res;
}

std::uint64_t estimate_clock_freq(rte_be16_t port_id) noexcept;
std::uint64_t estimate_clock_freq(rte_be16_t port_id, bool& swap) noexcept;

} // namespace dpdkx





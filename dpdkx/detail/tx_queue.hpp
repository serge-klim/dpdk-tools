#pragma once
#include "netinet_in.hpp"
#include "rte_ether.h"
#include <memory>
#include <string>
#include <cstddef>
#include <cstdint>


struct rte_mempool;
class device;

namespace dpdkx {

using queue_id_t = std::uint16_t;
constexpr auto default_ring_size() noexcept { return std::uint16_t{ 1024/*RX_RING_SIZE*/ }; }
class tx_queue{
public:
    tx_queue(device& device, queue_id_t queue_id, std::uint16_t ring_size = default_ring_size());
    tx_queue(tx_queue const&) = delete;
    tx_queue& operator=(tx_queue const&) = delete;
    tx_queue(tx_queue&& other) { ring_ = std::exchange(other.ring_, nullptr); }
    tx_queue& operator=(tx_queue&& other) { ring_ = std::exchange(other.ring_, nullptr); return *this; }
    ~tx_queue() noexcept;
    constexpr struct rte_ring* ring() noexcept { return ring_; }
    std::uint16_t enque(struct rte_mbuf** buffers, std::uint16_t n) noexcept;
    bool enque(struct rte_mbuf* buffer) noexcept;
private:
    struct rte_ring* ring_ /*= nullptr*/;
};
    

} // namespace dpdkx





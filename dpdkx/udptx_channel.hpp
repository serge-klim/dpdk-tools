#pragma once
#include "mempool.hpp"
#include "netinet_in.hpp"
#include "rte_ether.h"
#include <atomic>
#include <tuple>
#include <cstddef>
#include <cstdint>


struct rte_mempool;

namespace dpdkx {
	
class device;
class updtx_channel{
    static constexpr std::size_t cache_size = 32;    
public:
    updtx_channel(dpdkx::device& device, unsigned int socket_id, std::uint16_t n_packets = 8192, std::uint16_t packet_buf_size = RTE_MBUF_DEFAULT_BUF_SIZE);
    bool send(std::tuple<rte_ether_addr, rte_be32_t, rte_be16_t> const& dst_addr, void const* payload, std::size_t payload_size);
private:
    dpdkx::device& device_;
    rte_be16_t src_port_;
    std::uint16_t packet_buf_size_;
    static constexpr std::uint16_t header_buf_size_ = 2 * RTE_PKTMBUF_HEADROOM;
    rte_mempool* /*scoped_mempool*/ packet_pool_;
    rte_mempool* /*scoped_mempool*/ header_pool_;
    std::atomic<std::uint16_t> packet_id_ = 0;
};
    

} // namespace dpdkx



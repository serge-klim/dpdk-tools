#include "udptx_channel.hpp"
#include "device.hpp"
#include "error.hpp"
#include "rte_udp.h"
#include "rte_ip.h"
#include "error.hpp"
#include "rte_ether.h"  // ethernet header  - rte_ether_hdr
#include "rte_ethdev.h"
#include <system_error>
#include <utility>
#include <cassert>

dpdkx::updtx_channel::updtx_channel(dpdkx::device& device, unsigned int socket_id, std::uint16_t n_packets /*= 8192*/, std::uint16_t packet_buf_size /*= RTE_MBUF_DEFAULT_BUF_SIZE*/)
    : device_{device}
    , src_port_{ rte_cpu_to_be_16(device.next_src_port())} {

    const auto name = std::to_string(device.port_id()) + ':' + std::to_string(rte_be16_t(src_port_));

    //packet_pool_ = dpdkx::make_scoped_mempool(("packet_pool_" + name).c_str(), n_packets, cache_size, 0, packet_buf_size, static_cast<int>(socket_id));
    //packet_buf_size_ = rte_pktmbuf_data_room_size(packet_pool_.get()) - RTE_PKTMBUF_HEADROOM;
    //header_pool_ = dpdkx::make_scoped_mempool(("header_pool_" + name).c_str(), n_packets, cache_size, 0, header_buf_size_, static_cast<int>(socket_id));

    packet_pool_ = rte_pktmbuf_pool_create(("packet_pool_" + name).c_str(), n_packets, cache_size, 0, packet_buf_size, static_cast<int>(socket_id));
    if (!packet_pool_)
        throw std::system_error{ dpdkx::last_error(), "unable to create: packet_pool_" + name };

    packet_buf_size_ = rte_pktmbuf_data_room_size(packet_pool_) - RTE_PKTMBUF_HEADROOM;

    header_pool_ = rte_pktmbuf_pool_create(("header_pool_" + name).c_str(), n_packets, cache_size, 0, header_buf_size_, static_cast<int>(socket_id));
    if (!header_pool_)
        throw std::system_error{ dpdkx::last_error(), "unable to create: header_pool_" + name };

}

bool dpdkx::updtx_channel::send(std::tuple<rte_ether_addr, rte_be32_t, rte_be16_t> const& dst_addr, void const* payload, std::size_t payload_size) {

    //assert(/* space available for data in the mbuf */ rte_pktmbuf_data_room_size(packet_pool_) - RTE_PKTMBUF_HEADROOM == packet_buf_size_);
    static auto constexpr headers_size = static_cast<rte_be16_t/*std::uint16_t*/>(sizeof(rte_ether_hdr) + sizeof(rte_ipv4_hdr)) + sizeof(rte_udp_hdr);
    static_assert(RTE_PKTMBUF_HEADROOM >= headers_size, "Oops seems it's not enough space in the headers buffer");
    auto const total_size = headers_size + payload_size;
    //auto head = ip4_mbuf{};
    //if (header_buf_size_ < total_size || (head.reset(rte_pktmbuf_alloc(header_pool_)), !head.get())) {
    //    head.reset(rte_pktmbuf_alloc(packet_pool_));
    //    if (unlikely(!head))
    //        return {};
    //}
    rte_mbuf* head = nullptr;
    if (packet_buf_size_ < total_size /*&& (head = rte_pktmbuf_alloc(header_pool_)) != 0*/) {
        auto last = head = rte_pktmbuf_alloc(header_pool_);
        if (unlikely(head == nullptr))
            return false;
        //if (header_buf_size_ > headers_size)
        //    std::memcpy(rte_pktmbuf_mtod_offset(last, char*, headers_size), payload, payload_size);
//        auto begin = ;
        head->data_len = headers_size;
        head->nb_segs = 1;
        auto left = payload_size;
        do {
            last->next = rte_pktmbuf_alloc(packet_pool_);
            if (unlikely(last->next == nullptr)) {
                rte_pktmbuf_free(head);
                return false;
            }
            ++head->nb_segs;
            last = last->next;
            last->data_len = left > packet_buf_size_ ? static_cast<decltype(left)>(packet_buf_size_) : left;
            std::memcpy(rte_pktmbuf_mtod(last, char*), std::next(static_cast<char const*>(payload), payload_size - left), last->data_len);
            left -= last->data_len;
            //left->next = nullptr;
        } while (left != 0);
    } else {
        if (unlikely((head = rte_pktmbuf_alloc(packet_pool_)) == nullptr))
            return false;
        head->nb_segs = 1;
        //head->next = nullptr;
        // TODO: init_headr<<<<<<<<<<<<<<<<<<<
        std::memcpy(rte_pktmbuf_mtod_offset(head, char*, headers_size), payload, payload_size);
        head->data_len = static_cast<decltype(head->data_len)>(headers_size + payload_size);
    }
    auto const dgram_len = static_cast<std::uint16_t>(sizeof(rte_udp_hdr) + payload_size);

    auto ethdr = rte_pktmbuf_mtod(head, rte_ether_hdr*);
    assert(ethdr);
    //rte_ether_addr_copy(&dev_addr_, &ethdr->src_addr);    
    ethdr->src_addr = device_.mac_addr();
    //rte_ether_addr_copy(&dest_ether_addr_, &ethdr->dst_addr);  
    ethdr->dst_addr = std::get<decltype(ethdr->dst_addr)>(dst_addr);
    //ethdr->ether_type = 0x0800;  
    ethdr->ether_type = rte_cpu_to_be_16(RTE_ETHER_TYPE_IPV4);
    auto ip4hdr = rte_pktmbuf_mtod_offset(head, rte_ipv4_hdr*, sizeof(rte_ether_hdr));
    assert(ip4hdr);
    ip4hdr->version_ihl = RTE_IPV4_VHL_DEF;
    ip4hdr->type_of_service = 0;
    ip4hdr->total_length =      // size of ipv4 header and everything that follows
        rte_cpu_to_be_16(static_cast<std::uint16_t>(sizeof(rte_ipv4_hdr)) + dgram_len);
    ip4hdr->packet_id = rte_cpu_to_be_16(packet_id_); //rte_cpu_to_be_16((flow_id + 1) % UINT16_MAX);
    ip4hdr->fragment_offset = 0;
    ip4hdr->time_to_live = IPDEFTTL;      // default 64
    ip4hdr->next_proto_id = IPPROTO_UDP;  // UDP packet follows
    ip4hdr->hdr_checksum = 0;
    ip4hdr->src_addr = device_.ip4addr()/*rte_cpu_to_be_32(src_addr_)*/;
    ip4hdr->dst_addr = std::get<decltype(ip4hdr->dst_addr)>(dst_addr);
    auto udphdr = rte_pktmbuf_mtod_offset(head, rte_udp_hdr*, sizeof(rte_ether_hdr) + sizeof(rte_ipv4_hdr));
    assert(udphdr);
    udphdr->dst_port = std::get<decltype(udphdr->dst_port)>(dst_addr);
    udphdr->src_port = src_port_;
    udphdr->dgram_len = rte_cpu_to_be_16(dgram_len);  // size of the UDP header and everything that follows
    udphdr->dgram_cksum = 0;
    
    head->l2_len = sizeof(*ethdr);
    head->l3_len = /*rte_ipv4_hdr_len(ip4hdr)*/sizeof(*ip4hdr);
    head->l4_len = sizeof(struct rte_udp_hdr);
    head->pkt_len = static_cast<decltype(head->pkt_len)>(head->l2_len + head->l3_len + head->l4_len + payload_size);

    auto ol_flags = head->ol_flags = 0;
    if (device_.tx_offloads() & RTE_ETH_TX_OFFLOAD_IPV4_CKSUM)
        ol_flags |= RTE_MBUF_F_TX_IPV4 | RTE_MBUF_F_TX_IP_CKSUM;
    else
        ip4hdr->hdr_checksum = rte_ipv4_cksum(ip4hdr); 

    if (device_.tx_offloads() & RTE_ETH_TX_OFFLOAD_UDP_CKSUM) {
        ol_flags |= RTE_MBUF_F_TX_IPV4 | RTE_MBUF_F_TX_UDP_CKSUM;
        udphdr->dgram_cksum = rte_ipv4_phdr_cksum(ip4hdr, ol_flags);
    }
    else
        udphdr->dgram_cksum = rte_ipv4_udptcp_cksum_mbuf(head, ip4hdr, head->l2_len + head->l3_len) // multi segment
                            /*rte_ipv4_udptcp_cksum(ip4hdr, udphdr)*/
        ;

    head->ol_flags = ol_flags;
    assert((rte_mbuf_sanity_check(head, 1),true));

    auto res = device_.raw_tx().enque(head);
    if(!res)
        rte_pktmbuf_free(head);
    return res;
}

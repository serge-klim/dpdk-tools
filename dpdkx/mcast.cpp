#include "device.hpp"
#include "mbuf.hpp"
#include "proto/igmp.hpp"
#include "detail/utils.hpp"


std::error_code dpdkx::device::mcast_group(rte_be32_t ip4addr, igm_record::igmp_type type) {
    if(unlikely(RTE_IS_IPV4_MCAST(ip4addr)))
        return make_error_code(std::errc::bad_address); //TODO: throw maybe????

    auto buffer = rte_pktmbuf_alloc(svc_mempool_);
    if (unlikely(!buffer))
        return make_error_code(std::errc::no_buffer_space);

    static constexpr auto ip_header_alert_options = rte_be32_t{ 0x00000494 };
    static constexpr auto payload_size = sizeof(igmpv3_mem_rep) + sizeof(igm_record) + sizeof(ip4addr);
    auto ip4hdr = make_ipv4(buffer, IPPROTO_IGMP, sizeof(ip_header_alert_options) + payload_size);
    buffer->l3_len += sizeof(ip_header_alert_options);
    constexpr auto dst_addr = RTE_IPV4(224, 0, 0, 22);
    auto ethdr = rte_pktmbuf_mtod_offset(buffer, rte_ether_hdr*, 0);
    ethdr->src_addr = mac_addr();
    ethdr->dst_addr = ether_addr_for_ipv4_mcast(dst_addr);
    ip4hdr->ihl += 1;
    ip4hdr->src_addr = this->ip4addr()/*rte_cpu_to_be_32(src_addr_)*/;
    ip4hdr->dst_addr = rte_be_to_cpu_32(dst_addr);
    ip4hdr->time_to_live = 1;
    auto ip_header_options = rte_pktmbuf_mtod_offset(buffer, void*, buffer->l2_len + sizeof(*ip4hdr));
    std::memcpy(ip_header_options, &ip_header_alert_options, sizeof(ip_header_alert_options));
    auto igmp_mr = rte_pktmbuf_mtod_offset(buffer, igmpv3_mem_rep*, buffer->l2_len + buffer->l3_len);
    igmp_mr->type = igmpv3_mem_rep::membership_report; //membership report
    igmp_mr->reserved1 = 0;
    igmp_mr->reserved2 = 0;
    igmp_mr->num_records = rte_cpu_to_be_16(1);
    auto rec = rte_pktmbuf_mtod_offset(buffer, igm_record*, buffer->l2_len + buffer->l3_len + sizeof(*igmp_mr));
    rec->type = type;
    rec->len = 0;
    rec->num_sources = 0;
    *rte_pktmbuf_mtod_offset(buffer, decltype(ip4addr)*, buffer->l2_len + buffer->l3_len  + sizeof(*igmp_mr) + sizeof(*rec)) = ip4addr;
    igmp_mr->cksum = 0;
    igmp_mr->cksum = ~rte_raw_cksum(igmp_mr, buffer->l3_len + payload_size);
    ip4hdr->hdr_checksum = 0;
    if (tx_offloads() & RTE_ETH_TX_OFFLOAD_IPV4_CKSUM)
        buffer->ol_flags = RTE_MBUF_F_TX_IPV4 | RTE_MBUF_F_TX_IP_CKSUM;
    else
        ip4hdr->hdr_checksum = rte_ipv4_cksum(ip4hdr);
    return unlikely(!service_tx().enque(buffer)) ? make_error_code(std::errc::resource_unavailable_try_again/*device_or_resource_busy*/) : std::error_code{};
}


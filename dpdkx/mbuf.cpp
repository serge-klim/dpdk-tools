#include "mbuf.hpp"
#include "netinet_in.hpp"
#include "rte_mbuf_core.h"
#include "rte_ether.h"
#include "rte_ip.h"
#include "rte_udp.h"
#include <limits>
#include <cassert>


//https://doc.dpdk.org/guides/prog_guide/mbuf_lib.html/
//14.6. Meta Information

rte_ether_hdr* dpdkx::make_ether(rte_mbuf* buffer, rte_be16_t ether_type, std::size_t payload_size) {
	auto constexpr header_size = static_cast<rte_be16_t/*std::uint16_t*/>(sizeof(rte_ether_hdr));
	auto const total_size = header_size + payload_size;
	buffer->nb_segs = 1;
	buffer->next = nullptr;

	auto ethdr = rte_pktmbuf_mtod(buffer, rte_ether_hdr*);
	ethdr->ether_type = ether_type;
	buffer->l2_len = header_size;
	buffer->data_len = buffer->pkt_len = static_cast<decltype(buffer->data_len)>(total_size);
    buffer->ol_flags = 0;
    return ethdr;
}

rte_ipv4_hdr* dpdkx::make_ipv4(rte_mbuf* buffer, std::uint8_t next_proto_id, std::size_t payload_size) {
	auto const total_size = sizeof(rte_ipv4_hdr) + payload_size;
	[[maybe_unused]] auto ethdr = make_ether(buffer, rte_cpu_to_be_16(RTE_ETHER_TYPE_IPV4), total_size);

    auto ip4hdr = rte_pktmbuf_mtod_offset(buffer, rte_ipv4_hdr*, buffer->l2_len);
    ip4hdr->version_ihl = RTE_IPV4_VHL_DEF;
    ip4hdr->type_of_service = 0;
    ip4hdr->total_length =      // size of ipv4 header and everything that follows
        rte_cpu_to_be_16(static_cast<std::uint16_t>(sizeof(rte_ipv4_hdr)) + payload_size);
    ip4hdr->packet_id = 0; //rte_cpu_to_be_16((flow_id + 1) % UINT16_MAX);
    ip4hdr->fragment_offset = 0;
    ip4hdr->time_to_live = IPDEFTTL;      // default 64
    assert((ip4hdr->next_proto_id = (std::numeric_limits<decltype(ip4hdr->next_proto_id)>::max)()) != 0);
    ip4hdr->next_proto_id = next_proto_id /*IPPROTO_UDP*/;  // UDP packet follows
    ip4hdr->hdr_checksum = 0/*rte_ipv4_cksum(ip4hdr)*/; // checksum will be offloaded to NIC;

    buffer->l3_len = /*rte_ipv4_hdr_len(ip4hdr)*/sizeof(*ip4hdr);
    assert(buffer->l2_len == sizeof(*ethdr));
    assert(buffer->data_len == buffer->l2_len + buffer->l3_len + payload_size);
    assert(buffer->data_len == buffer->pkt_len);
//    assert((rte_mbuf_sanity_check(buffer, 1), true));
    return ip4hdr;
}

rte_udp_hdr* dpdkx::make_udp(rte_mbuf* buffer, std::size_t payload_size) {

    auto const total_size = sizeof(rte_udp_hdr) + payload_size;
    /*auto ip4hdr = */make_ipv4(buffer, IPPROTO_UDP, total_size);
    auto udphdr = rte_pktmbuf_mtod_offset(buffer, rte_udp_hdr*, buffer->l2_len + buffer->l3_len);
    assert(udphdr);
    udphdr->dgram_len = rte_cpu_to_be_16(total_size);  // size of the UDP header and everything that follows
    udphdr->dgram_cksum = 0;
    buffer->l4_len = sizeof(struct rte_udp_hdr);

    assert(buffer->l2_len == sizeof(rte_ether_hdr));
    assert(buffer->l3_len == /*rte_ipv4_hdr_len(ip4hdr)*/sizeof(rte_ipv4_hdr));
    assert(buffer->l4_len == sizeof(struct rte_udp_hdr));
    assert(buffer->pkt_len == buffer->l2_len + buffer->l3_len + buffer->l4_len + payload_size);
//    assert((rte_mbuf_sanity_check(buffer, 1), true));
    return udphdr;
}


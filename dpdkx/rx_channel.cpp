#include "rx_channel.hpp"
#include "device.hpp"
#include "rte_ip.h"
#include "rte_udp.h"
#include "rte_mbuf.h"
#include <system_error>
#include <cstdint>
#include <cassert>

dpdkx::endpoint dpdkx::make_endpoint(sockaddr_in const& addr) {
	if (addr.sin_family != AF_INET)
		throw std::system_error{ make_error_code(std::errc::address_family_not_supported) , "can't attach rx channel" };
	return { addr.sin_addr.s_addr, addr.sin_port };
}

dpdkx::rx_channel::rx_channel(use_make_rx_channel, dpdkx::device& dev, sockaddr_in const& ip_addr)
	: dev_{dev} {
	assert(ip_addr.sin_family == AF_INET && "only ip4 supported at the moment");
	//if (ip_addr.sin_family != AF_INET)
	//	throw std::system_error{ make_error_code(std::errc::address_family_not_supported) , "can't attach rx channel" };

	//if (auto error = join(ip_addr))
	//	throw std::system_error{ error , "can't attach rx channel" };
}

std::error_code dpdkx::rx_channel::join(endpoint ep) {
	return dev_.attach_rx(std::move(ep), shared_from_this());
}


bool dpdkx::rx_channel::enqueue(queue_id_t /*queue_id*/, rte_ipv4_hdr* ipv4hdr, rte_mbuf* buffer) {
	assert(ipv4hdr != nullptr);
	[[maybe_unused]] sockaddr_in from;
	from.sin_family = AF_INET;
	from.sin_addr.s_addr = ipv4hdr->dst_addr;
	auto udphdr = rte_pktmbuf_mtod_offset(buffer, rte_udp_hdr*, buffer->l2_len + buffer->l3_len);
	from.sin_port = udphdr->dst_port;
	[[maybe_unused]] auto const hdr_size = sizeof(*udphdr);
	//auto data = rte_pktmbuf_mtod_offset(buffer, char const*, buffer->l2_len + buffer->l3_len + hdr_size);
	assert(rte_be_to_cpu_16(udphdr->dgram_len) > hdr_size);
	//auto payload_size = rte_be_to_cpu_16(udphdr->dgram_len) - hdr_size;
	return false;
}


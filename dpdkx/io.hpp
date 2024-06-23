#pragma once
#include "config/device.hpp"
#include "rte_ethdev.h"
#include <iosfwd>
#include <tuple>
#include <string>
#include <cstdint>

namespace dpdkx { inline namespace v0 { 

namespace io {

enum tx_offloads : std::uint64_t {};
enum rx_offloads : std::uint64_t {};
enum rx_meta_features : std::uint64_t {};

std::string to_string(tx_offloads offloads);
std::string to_string(rx_offloads offloads);
std::string to_string(rx_meta_features features);

std::ostream& operator << (std::ostream& out, rx_offloads const& val);
std::ostream& operator << (std::ostream& out, tx_offloads const& val);
std::ostream& operator << (std::ostream& out, rx_meta_features const& val);

/**
 * Rx offload capabilities of a device.
 */
static inline constexpr char const* rx_offloads_names[] = {
	"RTE_ETH_RX_OFFLOAD_VLAN_STRIP",       // RTE_BIT64(0)
	"RTE_ETH_RX_OFFLOAD_IPV4_CKSUM",       // RTE_BIT64(1)
	"RTE_ETH_RX_OFFLOAD_UDP_CKSUM",        // RTE_BIT64(2)
	"RTE_ETH_RX_OFFLOAD_TCP_CKSUM",        // RTE_BIT64(3)
	"RTE_ETH_RX_OFFLOAD_TCP_LRO",          // RTE_BIT64(4)
	"RTE_ETH_RX_OFFLOAD_QINQ_STRIP",       // RTE_BIT64(5)
	"RTE_ETH_RX_OFFLOAD_OUTER_IPV4_CKSUM", // RTE_BIT64(6)
	"RTE_ETH_RX_OFFLOAD_MACSEC_STRIP",     // RTE_BIT64(7)
	"0x00100",
	"RTE_ETH_RX_OFFLOAD_VLAN_FILTER", // RTE_BIT64(9)
	"RTE_ETH_RX_OFFLOAD_VLAN_EXTEND", // RTE_BIT64(10)
	"0x00800",
	"0x01000",
	"RTE_ETH_RX_OFFLOAD_SCATTER", // RTE_BIT64(13)
	/**
	 * Timestamp is set by the driver in RTE_MBUF_DYNFIELD_TIMESTAMP_NAME
	 * and RTE_MBUF_DYNFLAG_RX_TIMESTAMP_NAME is set in ol_flags.
	 * The mbuf field and flag are registered when the offload is configured.
	 */
	"RTE_ETH_RX_OFFLOAD_TIMESTAMP",       // RTE_BIT64(14)
	"RTE_ETH_RX_OFFLOAD_SECURITY",        // RTE_BIT64(15)
	"RTE_ETH_RX_OFFLOAD_KEEP_CRC",        // RTE_BIT64(16)
	"RTE_ETH_RX_OFFLOAD_SCTP_CKSUM",      // RTE_BIT64(17)
	"RTE_ETH_RX_OFFLOAD_OUTER_UDP_CKSUM", // RTE_BIT64(18)
	"RTE_ETH_RX_OFFLOAD_RSS_HASH",        // RTE_BIT64(19)
	"RTE_ETH_RX_OFFLOAD_BUFFER_SPLIT",    // RTE_BIT64(20)
};

/**
* Tx offload capabilities of a device.
*/
static constexpr char const* tx_offloads_names[] = {
	"RTE_ETH_TX_OFFLOAD_VLAN_INSERT",//RTE_BIT64(0) 
	"RTE_ETH_TX_OFFLOAD_IPV4_CKSUM",//RTE_BIT64(1)
	"RTE_ETH_TX_OFFLOAD_UDP_CKSUM",//RTE_BIT64(2)
	"RTE_ETH_TX_OFFLOAD_TCP_CKSUM",//RTE_BIT64(3)
	"RTE_ETH_TX_OFFLOAD_SCTP_CKSUM",//RTE_BIT64(4)
	"RTE_ETH_TX_OFFLOAD_TCP_TSO",//RTE_BIT64(5)
	"RTE_ETH_TX_OFFLOAD_UDP_TSO",//RTE_BIT64(6)
	"RTE_ETH_TX_OFFLOAD_OUTER_IPV4_CKSUM",//RTE_BIT64(7)/**<Usedfortunnelingpacket.*/
	"RTE_ETH_TX_OFFLOAD_QINQ_INSERT",//RTE_BIT64(8)
	"RTE_ETH_TX_OFFLOAD_VXLAN_TNL_TSO",//RTE_BIT64(9)/**<Usedfortunnelingpacket.*/
	"RTE_ETH_TX_OFFLOAD_GRE_TNL_TSO",//RTE_BIT64(10)/**<Usedfortunnelingpacket.*/
	"RTE_ETH_TX_OFFLOAD_IPIP_TNL_TSO",//RTE_BIT64(11)/**<Usedfortunnelingpacket.*/
	"RTE_ETH_TX_OFFLOAD_GENEVE_TNL_TSO",//RTE_BIT64(12)/**<Usedfortunnelingpacket.*/
	"RTE_ETH_TX_OFFLOAD_MACSEC_INSERT",//RTE_BIT64(13)
	/**
		* Multiple threads can invoke rte_eth_tx_burst() concurrently on the same
		* Tx queue without SW lock.
		*/
	"RTE_ETH_TX_OFFLOAD_MT_LOCKFREE",//RTE_BIT64(14)
	/** Device supports multi segment send. */
	"RTE_ETH_TX_OFFLOAD_MULTI_SEGS",//RTE_BIT64(15)
	/**
		* Device supports optimization for fast release of mbufs.
		* When set application must guarantee that per-queue all mbufs comes from
		* the same mempool and has refcnt = 1.
		*/
	"RTE_ETH_TX_OFFLOAD_MBUF_FAST_FREE",//RTE_BIT64(16)
	"RTE_ETH_TX_OFFLOAD_SECURITY",//RTE_BIT64(17)
	/**
		* Device supports generic UDP tunneled packet TSO.
		* Application must set RTE_MBUF_F_TX_TUNNEL_UDP and other mbuf fields required
		* for tunnel TSO.
		*/
	"RTE_ETH_TX_OFFLOAD_UDP_TNL_TSO",//RTE_BIT64(18)
	/**
		* Device supports generic IP tunneled packet TSO.
		* Application must set RTE_MBUF_F_TX_TUNNEL_IP and other mbuf fields required
		* for tunnel TSO.
		*/
	"RTE_ETH_TX_OFFLOAD_IP_TNL_TSO",//RTE_BIT64(19)
	/** Device supports outer UDP checksum */
	"RTE_ETH_TX_OFFLOAD_OUTER_UDP_CKSUM",//RTE_BIT64(20)
	/**
		* Device sends on time read from RTE_MBUF_DYNFIELD_TIMESTAMP_NAME
		* if RTE_MBUF_DYNFLAG_TX_TIMESTAMP_NAME is set in ol_flags.
		* The mbuf field and flag are registered when the offload is configured.
		*/
	"RTE_ETH_TX_OFFLOAD_SEND_ON_TIMESTAMP",//RTE_BIT64(21)
};

static constexpr char const* rx_meta_features_names[] = {
	"RTE_ETH_RX_METADATA_USER_FLAG",//RTE_BIT64(0) 
	"RTE_ETH_RX_METADATA_USER_MARK",//RTE_BIT64(1)
	"RTE_ETH_RX_METADATA_TUNNEL_ID",//RTE_BIT64(2)
};

} /*inline namespace v0*/  } // namespace io

class device;

} // namespace dpdkx

namespace io {

	template<typename T>
	struct inner_type { using type = T; };

	template<>
	struct inner_type<unsigned char> { using type = unsigned int; };

	template<auto P> struct io_type;

	template<typename T, typename U, U T::* P> struct io_type<P> {
		using type = typename inner_type<U>::type;
	};

	template<> struct io_type<&rte_eth_dev_info::rx_offload_capa> {
		using type = dpdkx::io::rx_offloads;
	};

	template<> struct io_type<&rte_eth_dev_info::tx_offload_capa> {
		using type = dpdkx::io::tx_offloads;
	};

	template<> struct io_type<&rte_eth_dev_info::rx_queue_offload_capa> {
		using type = dpdkx::io::rx_offloads;
	};

	template<> struct io_type<&rte_eth_dev_info::tx_queue_offload_capa> {
		using type = dpdkx::io::tx_offloads;
	};

	template<> struct io_type<&dpdkx::config::device::effective_offloads::rx> {
		using type = dpdkx::io::rx_offloads;
	};

	template<> struct io_type<&dpdkx::config::device::effective_offloads::tx> {
		using type = dpdkx::io::tx_offloads;
	};

	template<> struct io_type<&dpdkx::config::device::rx_meta_features> {
		using type = dpdkx::io::rx_meta_features;
	};
} // namespace io


std::ostream& operator << (std::ostream& out, std::tuple<dpdkx::device const&, struct rte_eth_stats const&> const& v);



#include "utils/flags/parser/flags.hpp"
#include "parsers/parser.hpp"

template <>
struct parser::type<dpdkx::io::rx_offloads, std::true_type>
{
	auto const& operator()() noexcept {
		static const auto flag_parser = parser::make_flags_parser<std::uint64_t>(dpdkx::io::rx_offloads_names);
		return flag_parser;
	}
};

template <>
struct parser::type<dpdkx::io::tx_offloads, std::true_type>
{
	auto const& operator()() noexcept {
		static const auto flag_parser = parser::make_flags_parser<std::uint64_t>(dpdkx::io::tx_offloads_names);
		return flag_parser;
	}
};

template <>
struct parser::type<dpdkx::io::rx_meta_features, std::true_type>
{
	auto const& operator()() noexcept {
		static const auto flag_parser = parser::make_flags_parser<std::uint64_t>(dpdkx::io::rx_meta_features_names);
		return flag_parser;
	}
};

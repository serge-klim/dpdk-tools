#include "io.hpp"
#include "device.hpp"
#include "utils/flags/flags.hpp"
#include <ranges>
#include <iostream>

std::string dpdkx::v0::io::to_string(rx_offloads offloads) {
	return utils::flags::to_string(static_cast<std::uint64_t>(offloads), rx_offloads_names);
}

std::string dpdkx::v0::io::to_string(tx_offloads offloads) {
	return utils::flags::to_string(static_cast<std::uint64_t>(offloads), tx_offloads_names);
}

std::string dpdkx::v0::io::to_string(rx_meta_features features) {
    return utils::flags::to_string(static_cast<std::uint64_t>(features), rx_meta_features_names);
}

std::ostream& dpdkx::io::operator << (std::ostream& out, dpdkx::io::rx_offloads const& val) {
	out << std::hex << static_cast<std::underlying_type_t<dpdkx::io::rx_offloads>>(val);
	if (val)
		out << ':' << to_string(val);
	return out;
}

std::ostream& dpdkx::io::operator << (std::ostream& out, dpdkx::io::tx_offloads const& val) {
	out << std::hex << static_cast<std::underlying_type_t<dpdkx::io::rx_offloads>>(val);
	if (val)
		out << ':' << to_string(val);
	return out;
}

std::ostream& dpdkx::io::operator << (std::ostream& out, rx_meta_features const& features) {
    out << std::hex << static_cast<std::underlying_type_t<dpdkx::io::rx_meta_features>>(features);
    if (features)
        out << ':' << to_string(features);
    return out;
}

std::ostream& operator << (std::ostream& out, std::tuple<dpdkx::device const&, rte_eth_stats const&> const& v) {
    auto const& stats = std::get<rte_eth_stats const&>(v);
    auto rx_total = stats.ipackets + stats.imissed;
    auto rx_missed_pcnt = rx_total == 0 ? .0 : stats.imissed * 100.0 / rx_total;
    out << "\n\tTotal number of successfully received packets: " << stats.ipackets
        << "\n\tTotal number of successfully transmitted packets: " << stats.opackets
        << "\n\tTotal number of successfully received bytes: " << stats.ibytes
        << "\n\tTotal number of successfully transmitted bytes: " << stats.obytes
        /**
         * Total of Rx packets dropped by the HW,
         * because there are no available buffer (i.e. Rx queues are full).
         */
        << "\n\tmissed : " << stats.imissed << " from : " << stats.ipackets + stats.imissed << " - " << rx_missed_pcnt << '%'
        << "\n\tTotal number of erroneous received packets: " << stats.ierrors
        << "\n\tTotal number of failed transmitted packets: " << stats.oerrors
        << "\n\tTotal number of Rx mbuf allocation failures: " << stats.rx_nombuf
        ;

    auto const& device = std::get<dpdkx::device const&>(v);
    for (auto ix : std::ranges::iota_view(std::size_t{ 0 }, (std::min<std::size_t>)(RTE_ETHDEV_QUEUE_STAT_CNTRS, device.nrx())))
        out << "\n\t\t" << ix << " Total number of queue Rx packets : " << stats.q_ipackets[ix]
        << "\n\t\t" << ix << " Total number of successfully received queue bytes : " << stats.q_ibytes[ix]
        << "\n\t\t" << ix << " Total number of queue packets received that are dropped : " << stats.q_errors[ix];

    for (auto ix : std::ranges::iota_view(std::size_t{ 0 }, (std::min<std::size_t>)(RTE_ETHDEV_QUEUE_STAT_CNTRS, device.ntx())))
        out << "\n\t\t" << ix << " Total number of queue Tx packets : " << stats.q_opackets[ix]
        << "\n\t\t" << ix << " Total number of successfully transmitted queue bytes : " << stats.q_obytes[ix];

    return out;
}

#include "utils.hpp"
#include "rte_ethdev.h"
#include "rte_cycles.h"
#include <cstdint> 
#include <cassert>

std::uint64_t dpdkx::estimate_clock_freq(rte_be16_t port_id, bool& swap) noexcept {
	auto dev_ticks_base = std::uint64_t{};
	auto error = rte_eth_read_clock(port_id, &dev_ticks_base);
	auto rtds_ticks_base = rte_rdtsc_precise();
	if (unlikely(error != 0))
		return 0;
	/////////////////////////////////
	rte_delay_ms(100);
	std::uint64_t dev_ticks = 0;
	error = rte_eth_read_clock(port_id, &dev_ticks);
	auto rtds_ticks = rte_rdtsc();
	if (unlikely(error != 0))
		return 0;
	/////////////////////////////////
	//std::uint64_t dev_ticks, rtds_ticks;
	//do {
	//	rte_eth_read_clock(port_id, &dev_ticks);
	//	rtds_ticks = rte_rdtsc_precise();
	//} while (dev_ticks == dev_ticks_base || rtds_ticks == rtds_ticks_base);
	static auto constexpr shift = sizeof(dev_ticks) * 8 / 2;
	swap = (dev_ticks >> shift) != (dev_ticks_base >> shift);
	if (swap) {
		dev_ticks = rte_bswap64(dev_ticks);
		dev_ticks_base = rte_bswap64(dev_ticks_base);
	}
	assert(rtds_ticks - rtds_ticks_base != 0);
	return (rte_get_tsc_hz() * (dev_ticks - dev_ticks_base))/ (rtds_ticks - rtds_ticks_base);
}

std::uint64_t dpdkx::estimate_clock_freq(rte_be16_t port_id) noexcept {
	auto swap = false;
	return estimate_clock_freq(port_id, swap);
}


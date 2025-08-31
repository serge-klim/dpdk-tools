#include "sink.hpp"
#include "device.hpp"
#include "loggers.hpp"
#include "rte_ring.h"

unsigned int dpdkx::sink::setup_sink(struct rte_mbuf** buffers, std::uint16_t n) {
	auto core_id = rte_lcore_id();
	auto txq = device_.tx_queue(core_id);
	if (dpdkx::device::no_txq != txq) {
		id_.queue_id = txq;
		logging::logger<dpdkx::sink>().info("port {} sink (core {}) set to queue {} ", device_.port_id(), static_cast<int>(core_id), txq);
		ptr_ = &sink::tx_burst;
	} else {
		id_.ring = device_.tx_ring().ring();
		logging::logger<dpdkx::sink>().info("port {} sink (core {}) set to ring ", device_.port_id(), static_cast<int>(core_id));
		ptr_ = &sink::ring_enque;
	}	
	return enque(buffers, n);
}

unsigned int dpdkx::sink::ring_enque(struct rte_mbuf** buffers, std::uint16_t n) {
	return rte_ring_mp_enqueue_burst_elem(id_.ring, reinterpret_cast<void**>(buffers), sizeof(void*), n, nullptr);
}

unsigned int dpdkx::sink::tx_burst(struct rte_mbuf** buffers, std::uint16_t n) {
	return rte_eth_tx_burst(device_.port_id(), id_.queue_id, buffers, n);
}

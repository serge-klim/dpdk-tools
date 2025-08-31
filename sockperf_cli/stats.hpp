#pragma once
#include "dpdkx/detail/dpdk_type.hpp"
#include "dpdkx/memory.hpp"
#include "sockperf/sockperf.hpp"
#include <boost/program_options.hpp>
#include <numeric>
#include <vector>
#include <span>
#include <cstdint>

class tx_timestamps {   
 public:
   tx_timestamps(dpdkx::queue_id_t queue_id, std::size_t size);
   std::uint64_t& slot(sockperf::seqn_t) noexcept;
   constexpr std::size_t capacity() const noexcept { return capacity_; }
   using value_type = std::pair<sockperf::seqn_t, std::uint64_t>;
   constexpr std::span<value_type const> data() const noexcept { return {memory_.get(), next_}; } 
 private:
   std::size_t capacity_ = 0;
   dpdkx::rte_memory<value_type> memory_;
   value_type* next_ = nullptr;
};


class statistics {	
public:
	using size_type = std::size_t;
	statistics(size_type n_slots = 10000, std::uint8_t txq_n = 1);
	struct packet_info {
		sockperf::seqn_t seqn;
		//std::uint64_t sent;
		std::uint64_t received;
		std::uint64_t enqueued_rx;
        std::uint16_t size;
	};
	constexpr auto requested_slots() const noexcept { return next_slot_; }
	packet_info& slot();
    void process(boost::program_options::variables_map const& options, std::vector<std::pair<dpdkx::queue_id_t, std::span<tx_timestamps::value_type const>>> const& timestamps);
  private:
    packet_info const* data() const noexcept { return memory_.get(); }
  private:
	//using two_slots = std::array<packet_info, 2>;
	std::uint8_t txq_n_ ;
	size_type next_slot_ = 0;
	size_type size_ = 0;
    dpdkx::rte_memory<packet_info> memory_;
};


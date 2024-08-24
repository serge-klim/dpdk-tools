#pragma once
#include "sockperf/sockperf.hpp"
#include <boost/iostreams/device/mapped_file.hpp>
#include <boost/program_options.hpp>
#include <numeric>
#include <vector>
#include <array>
#include <variant>
#include <cstdint>


class statistics {	
public:
	using size_type = std::size_t;
	statistics(size_type n_slots = 10000, std::uint8_t txq_n = 1);
	struct packet_info {
		sockperf::seqn_t seqn;
		std::uint64_t sent;
		std::uint64_t received;
		std::uint64_t enqueued_rx;
        std::uint16_t size;
	};
	constexpr auto requested_slots() const noexcept { return next_slot_; }
	packet_info& slot(std::uint8_t queue);
	void process(boost::program_options::variables_map const& options);
private:
	using two_slots = std::array<packet_info, 2>;
	class mapped_file {
	public:
		mapped_file(std::uint8_t qid, size_type n_slots);
		packet_info* data() noexcept { return reinterpret_cast<statistics::packet_info*>(sink_.data()); }
	private:
		boost::iostreams::mapped_file_sink sink_;
	};
	std::uint8_t txq_n_ ;
	packet_info* slots_;
	size_type next_slot_ = 0;
	size_type size_ = 0;
	std::variant<two_slots, mapped_file> memory_;
};


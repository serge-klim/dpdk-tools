#pragma once
#include "sockperf/sockperf.hpp"
#include <boost/iostreams/device/mapped_file.hpp>
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
	};
	const auto& requested_slots(std::uint8_t queue) const noexcept { return buckets_[queue].next_slot; }
	auto requested_slots() const noexcept { return std::accumulate(cbegin(buckets_), cend(buckets_), 0, [](auto l, auto const& r) {return l + r.next_slot; }); }
	packet_info& slot(std::uint8_t queue);
	void process();
	packet_info const* front(std::uint8_t queue) const;
	packet_info const* back(std::uint8_t queue) const;
private:
	using two_slots = std::array<packet_info, 2>;
	class mapped_file {
	public:
		mapped_file(std::uint8_t qid, size_type n_slots);
		packet_info* data() noexcept { return reinterpret_cast<statistics::packet_info*>(sink_.data()); }
	private:
		boost::iostreams::mapped_file_sink sink_;
	};
	struct bucket {
		packet_info* slots;
		size_type next_slot = 0;
		size_type size = 0;
	};
	packet_info dummy_;
	std::array<bucket, 16> buckets_;
	std::vector<std::variant<two_slots, mapped_file>> buckets_memory_;
};


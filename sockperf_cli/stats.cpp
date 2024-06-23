#include "stats.hpp"
#include "loggers.hpp"
#include <boost/log/attributes/scoped_attribute.hpp>
#include <boost/process/environment.hpp>
#include <filesystem>
#include <algorithm>
#include <chrono>
#include <ranges>

statistics::mapped_file::mapped_file(std::uint8_t qid, size_type n_slots) {
	static const auto path = [] {
		auto path = std::filesystem::temp_directory_path() / "stats";
		std::filesystem::create_directories(path);
		path / "stats" / std::to_string(boost::this_process::get_id());
		return path.string() + '-';
	}();
	auto params = boost::iostreams::mapped_file_params{};
	params.path = path;
	params.path += std::to_string(qid);
	if (std::filesystem::exists(params.path))
		std::filesystem::remove(params.path);
	params.new_file_size = sizeof(statistics::packet_info) * n_slots;
	params.flags = boost::iostreams::mapped_file::mapmode::readwrite;
	sink_.open(params);
}


statistics::statistics(size_type n_slots /*= 1000*/, std::uint8_t txq_n /*= 1*/)  {
	//assert(n_slots != 0);
	for (auto bucket : std::ranges::views::iota(std::uint8_t{ 0 }, (std::min)(static_cast<std::size_t>(txq_n), buckets_.size()))) {
		if (n_slots > 2)
			buckets_memory_.emplace_back(mapped_file{bucket, n_slots});
		else
			buckets_memory_.emplace_back(two_slots{});
		buckets_[bucket].size = n_slots;
		buckets_[bucket].slots = std::visit([](auto& mem) {return mem.data(); }, buckets_memory_.back());
	}
}

statistics::packet_info& statistics::slot(std::uint8_t queue) {
	if (queue > buckets_.size() || buckets_[queue].size == 0) {
		return dummy_;
	}
	auto& [slots, ix, size] = buckets_[queue];
	auto& res = slots[size > ix ? ix : size - 1];
    ++ix;
    return res;
}


void statistics::process() {
	//auto next = sockperf::seqn_t{ 0 };
	for (auto const& [queue, bucket] : std::views::enumerate(buckets_)) {
		BOOST_LOG_SCOPED_LOGGER_TAG(stats_log::get(), "queue", queue);
		auto const& [slots, received_packets, size] = bucket;
        auto collected_packets = std::min(received_packets, size);
		switch (collected_packets) {
			default:{
			    auto first = slots[0].seqn;
				auto last = slots[collected_packets - 1].seqn;
			    auto out_of_sequence = sockperf::seqn_t{ 0 };
				auto missing = sockperf::seqn_t{ 0 };
                auto prev = size_type{0};
				for (size_type i = 1; i != collected_packets; ++i) {
					if (first > slots[i].seqn)
						first = slots[i].seqn;
					if (last < slots[i].seqn)
						last = slots[i].seqn;
					auto const expected = slots[prev].seqn + 1;
					if (expected != slots[i].seqn) {
						if (expected < slots[i].seqn){
							missing += slots[i].seqn - expected;
                            prev = i;                            
						} else {
							BOOST_LOG_SEV(stats_log::get(), boost::log::trivial::debug) /*<< n << ':'*/ << slots[i].seqn << " out of sequence " << slots[i - 1].seqn;
							++out_of_sequence;
							assert(missing != 0);
							missing--;
						}
					} else
                         prev = i;
					BOOST_LOG_SEV(stats_log::get(), boost::log::trivial::trace) << slots[i].seqn << ':' << slots[i].received << ':' << slots[i].enqueued_rx << ':' << slots[i].sent ;                    
				}
				auto expected = last - first + 1;
				if (missing != 0)
					BOOST_LOG_SEV(stats_log::get(), boost::log::trivial::warning) << missing << " packets are missing out of " << expected << " packets";
				if (out_of_sequence != 0) {
					BOOST_LOG_SEV(stats_log::get(), boost::log::trivial::warning) << out_of_sequence << " packets are out of order out of " << expected << " packets";
				}
				[[fallthrough]];
			}
			case 2: {
				auto ns = std::chrono::nanoseconds{ slots[collected_packets-1].received - slots[0].received };
                auto sec = std::chrono::duration_cast<std::chrono::duration<double,std::chrono::seconds::period>>(ns).count();
				auto enqueued_ns = std::chrono::nanoseconds{ slots[collected_packets - 1].enqueued_rx - slots[0].enqueued_rx };
				BOOST_LOG_SEV(stats_log::get(), boost::log::trivial::info) << received_packets << " packets has been received in "
                                                                           << std::fixed << sec << " sec."
                                                                           << " ~" << static_cast<unsigned long long>(received_packets / sec) << " packets per second";
				break;
			}
			case 1:
				[[fallthrough]];
			case 0:
				break;
		}
	}
}

statistics::packet_info const* statistics::front(std::uint8_t queue) const{
	return queue > buckets_.size() || buckets_[queue].size == 0
		? nullptr
		: buckets_[queue].slots;
}

statistics::packet_info const* statistics::back(std::uint8_t queue) const {
	return queue > buckets_.size() || buckets_[queue].size == 0
		? nullptr
		: buckets_[queue].slots + buckets_[queue].size - 1;
}

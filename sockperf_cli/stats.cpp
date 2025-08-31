#include "stats.hpp"
#include "loggers.hpp"
#include "sockperf/x.hpp"
#include "utils/workarounds.hpp"
#include "utils/histogram/chrono_axes.hpp"
#include <boost/histogram.hpp>
#include <boost/log/attributes/scoped_attribute.hpp>
#include <boost/process/v1/environment.hpp>
#include <format>
#include <algorithm>
#include <list>
#include <chrono>
#include <ranges>
#include <limits>
#include <type_traits>
#include <cassert>


tx_timestamps::tx_timestamps(dpdkx::queue_id_t queue_id, std::size_t size)
    : capacity_{size}
	, memory_{dpdkx::allocate_memory<value_type>(size, std::format("timestamps {}", queue_id).c_str()) }
	, next_{memory_.get()}   {
}

std::uint64_t& tx_timestamps::slot(sockperf::seqn_t seqn) noexcept {
   assert(next_ == memory_.get() || std::prev(next_)->first < seqn);
   assert(std::distance(memory_.get(), next_) < capacity());
   auto res = next_++;
   res->first = seqn;
   return res->second;
}

statistics::statistics(size_type n_slots /*= 1000*/, std::uint8_t txq_n /*= 1*/)
	: txq_n_{ txq_n }
  	, size_{ n_slots * txq_n }
	, memory_{dpdkx::allocate_memory<packet_info>(n_slots * txq_n, "stats")}{
}

statistics::packet_info& statistics::slot() {
    auto& res = memory_.get()[size_ > next_slot_ ? next_slot_ : size_ - 1];
    ++next_slot_;
    return res;
}


void statistics::process(boost::program_options::variables_map const& options, std::vector<std::pair<dpdkx::queue_id_t, std::span<tx_timestamps::value_type const>>> const& timestamps) {
    assert(txq_n_ == timestamps.size());
	//auto next = sockperf::seqn_t{ 0 };
	//BOOST_LOG_SCOPED_LOGGER_TAG(stats_log::get(), "queue", queue);
	auto const received_packets = requested_slots();
	//auto const& [slots, received_packets, size] = bucket;
    auto collected_packets = std::min(received_packets, size_);
    auto first = 0;
    auto last = collected_packets - 1;
	switch (collected_packets) {
		default:{
			auto const buckets_config = options["buckets"];
			auto buckets_p2p = buckets_config.empty()
				? std::set<double>{-100, -10, -5, 0, 1, 2, 3, 4, 5, 6, 7, 8, 9, 10, 11, 12, 13, 15, 20, 25, 30, 35, 40, 45, 50, 55, 60, 65, 70, 75, 80, 85, 90, 95, 100, 150, 200, 250, 300, 500, 500, 700, 800, 900, 1000, 1250, 1500}
			: utils::workarounds::to<std::set<double>>(buckets_config.as<std::list<std::chrono::nanoseconds>>() | std::views::transform([](auto const& duration) {
				return static_cast<double>(duration.count());
				})) /*| std::ranges::to<std::set<double>>()*/;

			auto histogram_p2p = boost::histogram::make_histogram(histogram::axis::duration<std::chrono::duration<double, std::nano>,
				boost::histogram::axis::variable<double,
				boost::histogram::use_default,
				boost::histogram::axis::option::growth_t>>(buckets_p2p)
			);

			//auto const buckets_config = options["buckets"];
			auto buckets = buckets_config.empty()
				? std::set<double>{-100, -10, -5, 0, 1, 2, 3, 4, 5, 6, 7, 8, 9, 10, 11, 12, 13, 15, 20, 25, 30, 35, 40, 45, 50, 55, 60, 65, 70, 75, 80, 85, 90, 95, 100, 150, 200, 250, 300, 500, 500, 700, 800, 900, 1000, 1250, 1500}
			: utils::workarounds::to<std::set<double>>(buckets_config.as<std::list<std::chrono::microseconds>>() | std::views::transform([](auto const& duration) {
				return static_cast<double>(duration.count());
				})) /*| std::ranges::to<std::set<double>>()*/;
            
			auto histogram = boost::histogram::make_histogram(histogram::axis::duration<std::chrono::duration<double, std::micro>,
				boost::histogram::axis::variable<double,
				boost::histogram::use_default,
				boost::histogram::axis::option::growth_t>>(buckets)
			);
            
			static constexpr auto not_set = (std::numeric_limits<size_type>::max)();
			struct per_queue {
				size_type out_of_sequence = 0;
				size_type missing = 0;
				size_type prev = not_set;
			};
			auto stats = std::vector<per_queue>{ txq_n_ };
			BOOST_LOG_SEV(stats_log::get(), boost::log::trivial::trace) << "tx queue ix: seqn : size : rx timestamp: rx timestamp(rtdsc) : tx timestamp(rtdsc) : p2p time(ns): rtt(ns)";

			for (size_type i = 0; i != collected_packets; ++i) {
				auto [queue, seqn] = sockperf::x::split_seqn(data()[i].seqn);
				BOOST_LOG_SCOPED_LOGGER_TAG(stats_log::get(), "queue", queue);
				if (stats.size() < queue) {
					BOOST_LOG_SEV(stats_log::get(), boost::log::trivial::warning) << "unexpected tx queue: " << static_cast<unsigned int>(queue) << " seqn: " << std::hex << "0x" << data()[i].seqn;
					continue;
				}
                auto ts = std::ranges::upper_bound(timestamps[queue].second, seqn - 1, {}, &tx_timestamps::value_type::first);
                assert(ts != begin(timestamps[queue].second));
				auto const sent = std::prev(ts)->second /*boost::endian::big_to_native(data()[i].sent)*/;
                auto const packet_rtt = std::chrono::nanoseconds{ data()[i].enqueued_rx - sent};
        		histogram(packet_rtt);
				if (data()[first].received > data()[i].received)
					first = i;
				if (data()[last].received < data()[i].received)
					last = i;
				if (stats[queue].prev == not_set) {
					stats[queue].prev = i;
					continue;
				}
				auto const expected = sockperf::x::split_seqn(data()[stats[queue].prev].seqn).second + 1;
				if (expected != seqn) {
					if (expected < seqn){
						//BOOST_LOG_SEV(stats_log::get(), boost::log::trivial::debug) << static_cast<unsigned int>(queue) << seqn << " expected " << expected;
						stats[queue].missing += seqn - expected;
						stats[queue].prev = i;
					} else {						
						BOOST_LOG_SEV(stats_log::get(), boost::log::trivial::debug) << static_cast<unsigned int>(queue)  << ':' << seqn << " out of sequence, expected : " << expected;
						++stats[queue].out_of_sequence;
						assert(stats[queue].missing != 0);
						stats[queue].missing--;
					}
				} else{
					stats[queue].prev = i;
				}
				assert(i != 0);
                auto const p2p_time = std::chrono::nanoseconds{ data()[i].received - data()[i-1].received };
				histogram_p2p(p2p_time);
				BOOST_LOG_SEV(stats_log::get(), boost::log::trivial::trace) << static_cast<unsigned int>(queue) << ':' << seqn << ':' << data()[i].size << ':' << data()[i].received << ':' << data()[i].enqueued_rx << ':' << sent << ':' << p2p_time.count() << ':' << packet_rtt.count();
			}
			//for (auto const& [queue, queue_stats] : std::views::enumerate(stats)) {
            for (auto queue = decltype(stats.size()){0}; queue != stats.size(); ++queue) {
				BOOST_LOG_SCOPED_LOGGER_TAG(stats_log::get(), "queue", queue);
                auto const& queue_stats = stats[queue];
				auto const expected = sockperf::x::split_seqn(data()[queue_stats.prev].seqn).second/*slots[last].seqn - slots[first].seqn + 1*/;
				if (queue_stats.missing != 0)
					BOOST_LOG_SEV(stats_log::get(), boost::log::trivial::warning) << "tx queue: " << static_cast<unsigned int>(queue) << ':' << queue_stats.missing << " packets are missing out of " << expected << " packets";
				if (queue_stats.out_of_sequence != 0) {
					BOOST_LOG_SEV(stats_log::get(), boost::log::trivial::warning) << "tx queue: " << static_cast<unsigned int>(queue) << ':' << queue_stats.out_of_sequence << " packets are out of order out of " << expected << " packets";
				}
			}
			auto out = std::stringstream{};
			out << "\npacket to packet interval "/*"(excluding gaps):"*/ << (data()[0].received == data()[0].enqueued_rx ? "(rdtsc)" :"(hardware timestamp)") << ":\n";
			dump(out, histogram_p2p);
            out << '\n';
			out << "packet round trip (rdtsc):\n";
			dump(out, histogram);
            out << '\n';            
			BOOST_LOG_SEV(stats_log::get(), boost::log::trivial::info) << out.str();
			[[fallthrough]];
		}
		case 2: {
            auto ns = std::chrono::nanoseconds{ data()[last].received - data()[first].received };
			if(ns.count()){
                //auto sec = std::chrono::duration_cast<std::chrono::duration<double,std::chrono::seconds::period>>(ns).count();
                auto packets_per_sec = received_packets * std::nano::den / ns.count();
                //auto enqueued_ns = std::chrono::nanoseconds{ slots[collected_packets - 1].enqueued_rx - slots[0].enqueued_rx };
                BOOST_LOG_SEV(stats_log::get(), boost::log::trivial::info) << received_packets << " packets has been received in "
                                                                        //<< std::fixed << sec << " sec."
                                                                        << std::chrono::duration_cast<std::chrono::microseconds>(ns).count() << " us "
                                                                        << "/ " << packets_per_sec << " packets per second"
                                                                        " (~" << static_cast<double>(data()[collected_packets-1].size * 8 * received_packets) / ns.count() << " Gbps)"
                                                                        ;
            }
			break;
		}
		case 1:
			[[fallthrough]];
		case 0:
			break;
	}
}


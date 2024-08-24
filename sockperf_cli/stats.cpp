#include "stats.hpp"
#include "loggers.hpp"
#include "sockperf/x.hpp"
#include "utils/workarounds.hpp"
#include "utils/histogram/chrono_axes.hpp"
#include <boost/histogram.hpp>
#include <boost/log/attributes/scoped_attribute.hpp>
#include <boost/process/environment.hpp>
#include <filesystem>
#include <algorithm>
#include <list>
#include <chrono>
#include <ranges>
#include <limits>

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


statistics::statistics(size_type n_slots /*= 1000*/, std::uint8_t txq_n /*= 1*/)
	: txq_n_{ txq_n }
  	, size_{ n_slots * txq_n }    
	, memory_{ n_slots > 2 ? std::variant<two_slots, mapped_file>{mapped_file{ 0, n_slots * txq_n }} : std::variant<two_slots, mapped_file>{ two_slots{} } }
{
	slots_ = std::visit([](auto& mem) {return mem.data(); }, memory_);
}

statistics::packet_info& statistics::slot(std::uint8_t /*queue*/) {
	auto& res = slots_[size_ > next_slot_ ? next_slot_ : size_ - 1];
    ++next_slot_;
    return res;
}


void statistics::process(boost::program_options::variables_map const& options) {
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
				auto [queue, seqn] = sockperf::x::split_seqn(slots_[i].seqn);
				BOOST_LOG_SCOPED_LOGGER_TAG(stats_log::get(), "queue", queue);
				if (stats.size() < queue) {
					BOOST_LOG_SEV(stats_log::get(), boost::log::trivial::warning) << "unexpected tx queue: " << static_cast<unsigned int>(queue) << " seqn: " << std::hex << "0x" << slots_[i].seqn;
					continue;
				}
				auto const sent = boost::endian::big_to_native(slots_[i].sent);
                auto const packet_rtt = std::chrono::nanoseconds{ slots_[i].enqueued_rx - sent};
        		histogram(packet_rtt);
				if (slots_[first].received > slots_[i].received)
					first = i;
				if (slots_[last].received < slots_[i].received)
					last = i;
				if (stats[queue].prev == not_set) {
					stats[queue].prev = i;
					continue;
				}
				auto const expected = sockperf::x::split_seqn(slots_[stats[queue].prev].seqn).second + 1;
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
					//histogram_p2p(std::chrono::nanoseconds{ slots_[i].received - slots_[stats[queue].prev].received });
					stats[queue].prev = i;
				}
				assert(i != 0);
                auto const p2p_time = std::chrono::nanoseconds{ slots_[i].received - slots_[i-1].received };
				histogram_p2p(p2p_time);
				BOOST_LOG_SEV(stats_log::get(), boost::log::trivial::trace) << static_cast<unsigned int>(queue) << ':' << seqn << ':' << slots_[i].size << ':' << slots_[i].received << ':' << slots_[i].enqueued_rx << ':' << sent << ':' << p2p_time.count() << ':' << packet_rtt.count();
			}
			for (auto const& [queue, queue_stats] : std::views::enumerate(stats)) {
				BOOST_LOG_SCOPED_LOGGER_TAG(stats_log::get(), "queue", queue);
				auto const expected = sockperf::x::split_seqn(slots_[queue_stats.prev].seqn).second/*slots[last].seqn - slots[first].seqn + 1*/;
				if (queue_stats.missing != 0)
					BOOST_LOG_SEV(stats_log::get(), boost::log::trivial::warning) << "tx queue: " << static_cast<unsigned int>(queue) << ':' << queue_stats.missing << " packets are missing out of " << expected << " packets";
				if (queue_stats.out_of_sequence != 0) {
					BOOST_LOG_SEV(stats_log::get(), boost::log::trivial::warning) << "tx queue: " << static_cast<unsigned int>(queue) << ':' << queue_stats.out_of_sequence << " packets are out of order out of " << expected << " packets";
				}
			}
			auto out = std::stringstream{};
			out << "\npacket to packet interval "/*"(excluding gaps):"*/ << (slots_[0].received == slots_[0].enqueued_rx ? "(rdtsc)" :"(hardware timestamp)") << ":\n";
			dump(out, histogram_p2p);
            out << '\n';
			out << "packet round trip (rdtsc):\n";
			dump(out, histogram);
            out << '\n';            
			BOOST_LOG_SEV(stats_log::get(), boost::log::trivial::info) << out.str();
			[[fallthrough]];
		}
		case 2: {
            auto ns = std::chrono::nanoseconds{ slots_[last].received - slots_[first].received };
			if(ns.count()){
                //auto sec = std::chrono::duration_cast<std::chrono::duration<double,std::chrono::seconds::period>>(ns).count();
                auto packets_per_sec = received_packets * std::nano::den / ns.count();
                //auto enqueued_ns = std::chrono::nanoseconds{ slots[collected_packets - 1].enqueued_rx - slots[0].enqueued_rx };
                BOOST_LOG_SEV(stats_log::get(), boost::log::trivial::info) << received_packets << " packets has been received in "
                                                                        //<< std::fixed << sec << " sec."
                                                                        << std::chrono::duration_cast<std::chrono::microseconds>(ns).count() << " us "
                                                                        << "/ " << packets_per_sec << " packets per second"
                                                                        " (~" << static_cast<double>(slots_[collected_packets-1].size * 8 * received_packets) / ns.count() << " Gbps)"
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


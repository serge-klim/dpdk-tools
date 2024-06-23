#pragma once
#include "stats.hpp"
#include "sockperf/x.hpp"
#include "dpdkx/jobs.hpp"
#include "dpdkx/rx_channel.hpp"
#include "dpdkx/mempool.hpp"
#include "utils/histogram/chrono_axes.hpp"
#include <boost/histogram.hpp>
#include <chrono>
#include <atomic>
#include <vector>

class tx_job : public dpdkx::job {
public:
    tx_job(dpdkx::device& device, dpdkx::queue_id_t queue_id, dpdkx::shared_mempool packet_pool, std::size_t packets2send, std::uint64_t ol_flags, std::size_t payload_size);
	dpdkx::job_state process() override;
    std::error_code warmup(class sockperf_channel& channel, std::vector<dpdkx::job*>& jobs, std::chrono::milliseconds timeout = std::chrono::duration_cast<std::chrono::milliseconds>(std::chrono::seconds{ 25 }));
private:
    dpdkx::device& device_;
    dpdkx::queue_id_t queue_id_;
    dpdkx::shared_mempool packet_pool_;
    std::size_t packets2send_ = 0;
    std::uint64_t ol_flags_ = 0;
    std::size_t payload_size_ = 0;
};

class latency_test_job : public dpdkx::job {
public:
    latency_test_job(sockperf_channel& rx_channel, dpdkx::queue_id_t queue_id, dpdkx::shared_mempool packet_pool, std::size_t packets2send, std::uint64_t ol_flags, std::size_t payload_size);
    dpdkx::job_state process() override;
private:
    sockperf_channel& rx_channel_;
    dpdkx::queue_id_t queue_id_;
    dpdkx::shared_mempool packet_pool_;
    std::size_t packets2send_ = 0;
    std::uint64_t ol_flags_ = 0;
    std::size_t payload_size_ = 0;
};

struct message_info {
    sockperf::seqn_t seqn = 0;
    //boost::endian::big_uint64_t originate_cpu_ms = 0;        //the time the sender last touched the message before sending it
    //boost::endian::big_uint64_t originate_nic = 0;
    std::uint64_t received_ts = 0; 
    std::uint64_t enqueued_ts = 0;
};


class sockperf_channel : public dpdkx::rx_channel {
public:
    sockperf_channel(use_make_rx_channel, dpdkx::device& device, sockaddr_in const& addr, std::size_t packets2send, bool detailed_stats = false);
    constexpr auto packets_received() const { return received_; }
    constexpr statistics& stats() noexcept { return stats_; }
    constexpr statistics const& stats() const noexcept { return stats_; }
    auto time() const noexcept { return std::make_pair(end_.first - start_.first, end_.second == (std::chrono::high_resolution_clock::time_point::min)() ? std::chrono::high_resolution_clock::duration::zero() : end_.second - start_.second); }
    auto last_warmup_message() const noexcept { return last_warmup_message_.load(); }
    bool enqueue(dpdkx::queue_id_t queue_id, rte_ipv4_hdr* ipv4hdr, rte_mbuf* buffer) override;
private:
    std::size_t packets2send_ = 0;    
    std::size_t received_ = 0;
    std::pair<std::uint64_t, std::chrono::high_resolution_clock::time_point> start_ = { 0, (std::chrono::high_resolution_clock::time_point::min)() };
    std::pair<std::uint64_t, std::chrono::high_resolution_clock::time_point> end_ = { 0, (std::chrono::high_resolution_clock::time_point::min)()};
    statistics stats_;
    std::atomic<std::shared_ptr<message_info>> last_warmup_message_;
};


std::pair<std::size_t, std::uint64_t> configure_sockperf_packet_pool(dpdkx::device& device, rte_mempool* packet_pool, std::size_t payload_size, std::pair<rte_be32_t, rte_ether_addr> const& dest, rte_be16_t dst_port, std::uint8_t time_to_live = 64);


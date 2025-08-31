#pragma once
#include "stats.hpp"
#include "sockperf/x.hpp"
#include "dpdkx/detail/atomic_shared_ptr.hpp"
#include "dpdkx/jobs.hpp"
#include "dpdkx/rx_channel.hpp"
#include "dpdkx/mempool.hpp"
#include <chrono>
#include <atomic>
#include <vector>

class tx_job : public dpdkx::job {
public:
    tx_job(dpdkx::device& device, dpdkx::queue_id_t queue_id, dpdkx::shared_mempool packet_pool, std::size_t packets2send, std::uint64_t ol_flags, std::size_t payload_size);
    constexpr dpdkx::queue_id_t queue_id() const noexcept { return queue_id_; }
    constexpr tx_timestamps const& timestamps() const noexcept { return timestamps_; } 
	dpdkx::job_state process() override;
    std::error_code warmup(class sockperf_channel& channel, std::vector<dpdkx::job*>& jobs, std::chrono::milliseconds timeout = std::chrono::duration_cast<std::chrono::milliseconds>(std::chrono::seconds{ 25 }));
private:
    dpdkx::device& device_;
    dpdkx::queue_id_t queue_id_;
    std::uint16_t burst_size_;
    std::vector<rte_mbuf*> buffers_;
    tx_timestamps timestamps_;
    dpdkx::shared_mempool packet_pool_;
    std::size_t packets2send_ = 0;
    std::uint64_t ol_flags_ = 0;
    std::size_t payload_size_ = 0;
};

class latency_test_job : public dpdkx::job {
public:
    latency_test_job(sockperf_channel& rx_channel, dpdkx::queue_id_t queue_id, dpdkx::shared_mempool packet_pool, 
        std::size_t packets2send, std::uint64_t ol_flags, std::size_t payload_size, bool disable_clock);
    dpdkx::job_state process() override;
private:
    sockperf_channel& rx_channel_;
    dpdkx::queue_id_t queue_id_;
    dpdkx::shared_mempool packet_pool_;
    std::size_t packets2send_ = 0;
    std::uint64_t ol_flags_ = 0;
    std::size_t payload_size_ = 0;
    bool disable_clock_ = false;
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
    sockperf_channel( dpdkx::device& device, sockaddr_in const& addr, std::size_t packets2send, bool detailed_stats = false);
    constexpr auto packets_received() const { return stats_.requested_slots(); }
    constexpr statistics& stats() noexcept { return stats_; }
    constexpr statistics const& stats() const noexcept { return stats_; }
    auto last_warmup_message() const noexcept { return workaround::load(last_warmup_message_); }
    std::uint16_t enqueue(dpdkx::queue_id_t queue_id, rte_mbuf** buffers, std::uint16_t n) override;
private:
    std::size_t packets2send_ = 0;    
    statistics stats_;
    workaround::atomic_shared_ptr<message_info> last_warmup_message_;
};


std::pair<std::size_t, std::uint64_t> configure_sockperf_packet_pool(dpdkx::device& device, rte_mempool* packet_pool, std::size_t payload_size, std::pair<rte_be32_t, rte_ether_addr> const& dest, rte_be16_t dst_port, std::uint8_t time_to_live = 64);


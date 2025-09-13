#pragma once
#include "jobs.hpp"
#include "proto/igmp.hpp"
#include "config/device.hpp"
#include "mempool.hpp"
#include "flow/rx_queue.hpp"
#include "detail/atomic_shared_ptr.hpp"
#include "detail/dpdk_type.hpp"
#include "detail/numerics.hpp"
#include "rx_channel.hpp"
#include "rte_memory.h"
#include "rte_byteorder.h"
#include "rte_mbuf.h"
//#include "rte_lpm.h"
#include "rte_ring.h"
#include "rte_ether.h"
#include "detail/tx_ring.hpp"
#include <boost/functional/hash.hpp>
#include <boost/unordered/concurrent_flat_map.hpp>

#include <atomic>
#include <array>
#include <vector>
#include <memory>
#include <tuple>
#include <mutex>
#include <unordered_map>
#include <utility>
#include <chrono>
#include <limits>
#include <cstdint>


//#include <boost/unordered/concurrent_flat_map.hpp>

struct rte_ring;

namespace dpdkx {

struct endpoint_hasher
{
    std::size_t operator()(ip4_endpoint const& ep) const noexcept
    {
        auto res = std::size_t{ 0 };
        boost::hash_combine(res, std::hash<decltype(ep.first)>{}(ep.first));
        boost::hash_combine(res, std::hash<decltype(ep.second)>{}(ep.second));
        return res;
    }
};

using unique_ring = std::unique_ptr<rte_ring, decltype(&rte_ring_free)>;
//struct sink {
//    virtual bool process(struct rte_mbuf* buffer) = 0;
//};

using sinks_t = std::vector<std::pair<ip4_endpoint, std::shared_ptr<rx_channel>/*unique_ring*/>>;
class device
{
    class tx_job : public job {
    public:
        tx_job(device& device, queue_id_t queue_id, std::uint16_t burts_size);
        job_state process() override;
        [[nodiscard]] constexpr dpdkx::tx_ring& tx_ring() noexcept { return queue_; }
    private:
        dpdkx::device& device_;
        queue_id_t queue_id_;
        std::uint16_t n_ = 0;
        std::uint16_t burst_size_ = 0;
        dpdkx::tx_ring queue_;
        std::array<rte_mbuf*, max_pkt_burst> buffers_;
    };

    class rx_job : public job {
    public:
        rx_job(device& device, queue_id_t queue_id, std::uint16_t burts_size, std::uint16_t reload_hint);
        job_state process() override;
    private:
        inline auto try_reload_sinks() noexcept {
            auto res = false;
            if( unlikely (checkpoint_ >= reload_checkpoint_)) {
                while (sinks_ != device_.sinks()) {
                    sinks_ = device_.sinks();
                    res = true;
                }
                checkpoint_ = 0;
            }
            return res;
        }
    private:
        device& device_;
        queue_id_t queue_id_;

        std::uint16_t last_ = 0;
        std::uint16_t checkpoint_ = 1001;
#pragma message("TODO:configure it based on burst_size")
        std::uint16_t reload_checkpoint_ = 1001;
        std::uint16_t burst_size_ = 0;
        std::array<rte_mbuf*, max_pkt_burst*2> buffers_;
        std::shared_ptr<sinks_t> sinks_;
    };
public:
    device(config::device config);
    ~device();
    std::string const& id() const noexcept { return config_.id; }
    constexpr port_id_t port_id() const noexcept { return config_.port_id; }
    constexpr unsigned int socket_id() const noexcept { return config_.socket_id; } // could be different to rte_eth_dev_socket_id(port_id)
    constexpr rte_ether_addr const& mac_addr() const noexcept { return config_.mac_addr; }
    constexpr rte_be32_t ip4addr() const noexcept { return config_.ip4_addr; }

    constexpr rte_eth_dev_info const& dev_info() const noexcept { return config_.info; }
    constexpr std::uint64_t rx_offloads() const noexcept { return config_./*info.rx_offload_capa*/effective_offload.rx; }
    constexpr std::uint64_t tx_offloads() const noexcept { return config_./*info.tx_offload_capa*/effective_offload.tx; }

    constexpr bool clock_bswap() const noexcept { return clock_bswap_; }
    constexpr bool clock_enabled() const noexcept { return config_.features[config::device::dev_clock];}
    rte_mbuf_timestamp_t read_clock() const noexcept;
    constexpr std::uint64_t clock_hz() const noexcept { return config_.clock_hz; }
    constexpr rte_mbuf_timestamp_t timestamp_fix(rte_mbuf_timestamp_t timestamp) const noexcept { return mul_div(timestamp, NS_PER_S, clock_hz()) /*timestamp * NS_PER_S / clock_hz()*/; }
    int link_status(rte_eth_link& status, std::chrono::milliseconds timeout) const noexcept;
    template<typename Rep, typename Period>
    int link_status(rte_eth_link& status, std::chrono::duration<Rep, Period> timeout) const noexcept { return link_status(status, std::chrono::duration_cast<std::chrono::milliseconds>(timeout));}
   
    std::error_code ether_address(rte_be32_t ip4addr, rte_ether_addr& mac_addr) noexcept;

    [[nodiscard]] std::shared_ptr<sinks_t> sinks() const noexcept { return workaround::load(sinks_,std::memory_order_relaxed); }
    [[nodiscard]] std::uint16_t next_src_port() noexcept;
//TODO: most likely should be replaced with raw_tx_channel() -> interface;
    [[deprecated]][[nodiscard]] dpdkx::tx_ring& tx_ring(queue_id_t hint = 0) noexcept;
    [[deprecated]][[nodiscard]] dpdkx::tx_ring& service_tx(queue_id_t hint = 0) noexcept;
    [[nodiscard]] std::uint16_t process_rx_packets(queue_id_t queue_id, rte_mbuf** buffers, std::uint16_t n);
///////////
    //device(device const&) = delete;
    //device& operator=(device const&) = delete;
    [[nodiscard]] auto nrx() const noexcept { return rx_jobs_.size(); }
    [[nodiscard]] auto ntx() const noexcept { return tx_jobs_.size(); }
    [[nodiscard]] std::vector<job*> rx_jobs();
    [[nodiscard]] std::vector<job*> tx_jobs();
    [[nodiscard]] std::vector<job*> jobs();
    int start() /*noexcept*/;
    int stop() noexcept;
    enum class attach_mode {
        any,
        rss,
        flow
    };
    std::error_code attach_rx(ip4_endpoint ep, std::shared_ptr<rx_channel> ch, attach_mode mode = attach_mode::any);
    std::error_code detach_rx(ip4_endpoint ep, std::shared_ptr<rx_channel> ch);
    std::error_code detach_rx(std::shared_ptr<rx_channel> ch);

    std::error_code join_mcast_group(rte_be32_t ip4addr) { return mcast_group(ip4addr, igm_record::change_to_exclude_mode); }
    std::error_code leave_mcast_group(rte_be32_t ip4addr) { return mcast_group(ip4addr, igm_record::change_to_include_mode); }
    constexpr bool has_timestamp(rte_mbuf const* mbuf) const noexcept { return (mbuf->ol_flags & dynfields_.timestamp.flag) != 0; }
    [[nodiscard]] rte_mbuf_timestamp_t* buffer_timestamp(rte_mbuf const* mbuf) const noexcept
    {
        return has_timestamp(mbuf)
                ? RTE_MBUF_DYNFIELD(mbuf, dynfields_.timestamp.offset, rte_mbuf_timestamp_t*)
                : nullptr;
    }
    static constexpr queue_id_t no_txq = (std::numeric_limits<queue_id_t>::max)();
    [[nodiscard]] dpdkx::queue_id_t tx_queue(core_t core_id) noexcept;
    [[nodiscard]] queue_id_t tx_queue() noexcept { return tx_queue(rte_lcore_id()); }
private:
    std::error_code mcast_group(rte_be32_t ip4addr, igm_record::igmp_type type);

    std::error_code join_mcast(rte_be32_t ip4addr);
    std::error_code leave_mcast(rte_be32_t ip4addr);
private:
    // static std::uint16_t rx_callback(port_id_t port_id, queue_id_t queue, struct rte_mbuf* pkts[], std::uint16_t nb_pkts, std::uint16_t max_pkts, void* user_param);
private:
    //rte_mbuf_timestamp_t clock_adjustment_ = 0;
    rte_mempool* svc_mempool_ = nullptr;
    scoped_mempool rx_mbuf_pool_;
    struct {
        struct {
            std::uint64_t flag = 0;
            int offset = -1;
        }timestamp;
    } dynfields_;
    std::vector<rx_job> rx_jobs_;
    std::vector<tx_job> tx_jobs_;
    std::vector<flow::rx_queue> fqueues_;
    config::device config_;
    bool clock_bswap_ = false;
//TODO:wrap in mcast tracker:
    std::mutex mcast_guard_;
    std::unordered_map<rte_be32_t, std::uint8_t> joined_mcasts_;
    struct arp4rec {
        rte_ether_addr mac_addr = {};
        std::uint64_t timestamp = 0;
    };
    boost::concurrent_flat_map<rte_be32_t, arp4rec> arp4_;
/////////////////
    workaround::atomic_shared_ptr<sinks_t> sinks_;
    std::atomic<std::uint16_t> next_src_port_;
    std::uint16_t tx_queues_ = 0;
    std::array<queue_id_t, RTE_MAX_LCORE> per_core_queue_ = { 0 };
};

[[nodiscard]] std::string svc_mempool_name(unsigned int socket_id);

inline rte_mbuf_timestamp_t timestamp(device const& device, struct rte_mbuf const* mbuf) noexcept {
    auto res = device.buffer_timestamp(mbuf);
    return res ? device.timestamp_fix(*res) : device.read_clock();
}

static_assert(!std::is_copy_constructible_v<device>, "device copy constructor should be disabled");
static_assert(!std::is_copy_assignable_v<device>, "device shouldn't be copy assignable ");

} // namespace dpdkx


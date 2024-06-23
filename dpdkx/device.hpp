#pragma once
#include "jobs.hpp"
#include "proto/igmp.hpp"
#include "config/device.hpp"
#include "mempool.hpp"
#include "rx_channel.hpp"
#include "rte_memory.h"
#include "rte_byteorder.h"
//#include "rte_lpm.h"
#include "rte_ring.h"
#include "rte_ether.h"
#include "detail/tx_queue.hpp"
#include <boost/functional/hash.hpp>
#include <boost/unordered/concurrent_flat_map.hpp>

#include <atomic>
#include <array>
#include <vector>
#include <memory>
#include <cstdint>
#include <tuple>
#include <atomic>
#include <mutex>
#include <unordered_map>
#include <utility>
#include <chrono>
#include <limits>
#include <cstdint>


//#include <boost/unordered/concurrent_flat_map.hpp>

struct rte_ring;
struct rte_mbuf;

namespace dpdkx {

struct endpoint_hasher
{
    std::size_t operator()(endpoint const& ep) const noexcept
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

using sinks_t = std::vector<std::pair<endpoint, std::shared_ptr<rx_channel>/*unique_ring*/>>;
constexpr std::size_t max_pkt_burst = 512;
class device
{
    class tx_job : public job {
    public:
        tx_job(device& device, queue_id_t queue_id, std::uint16_t burts_size);
        job_state process() override;
        [[nodiscard]] constexpr tx_queue& raw_tx() noexcept { return queue_; }
    private:
        dpdkx::device& device_;
        port_id_t port_id_;
        queue_id_t queue_id_;
        std::uint16_t n_ = 0;
        std::uint16_t burst_size_ = 0;
        tx_queue queue_;
        std::array<rte_mbuf*, max_pkt_burst> buffers_;
    };

    class rx_job : public job {
    public:
        rx_job(device& device, queue_id_t queue_id, std::uint16_t burts_size, std::uint16_t reload_hint);
        job_state process() override;
    private:
        inline auto try_reload_sinks() noexcept {
            auto res = false;
            if (checkpoint_ >= reload_checkpoint_) {
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
        port_id_t port_id_;
        queue_id_t queue_id_;

        std::uint16_t last_ = 0;
        std::uint16_t checkpoint_ = 1001;
#pragma message("TODO:configure it based on burst_size")
        std::uint16_t reload_checkpoint_ = 1001;
        std::uint16_t burst_size_ = 0;
        std::array<rte_mbuf*, max_pkt_burst> buffers_;
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
    constexpr rte_mbuf_timestamp_t timestamp_fix(rte_mbuf_timestamp_t timestamp) const noexcept { return timestamp * NS_PER_S / clock_hz(); }
    //constexpr rte_mbuf_timestamp_t timestamp_fix(rte_mbuf_timestamp_t timestamp) const noexcept { 
    //    if (std::get<bool>(clock_adjustment_))
    //        timestamp = rte_bswap64(timestamp);
    //    return static_cast<rte_mbuf_timestamp_t>(timestamp * std::get<double>(clock_adjustment_)); 
    //}
    std::optional<rte_eth_link> link_status() const noexcept;
    std::optional<rte_eth_link> link_status(std::chrono::milliseconds timeout) const;
    template<typename Rep, typename Period>
    std::optional<rte_eth_link> link_status(std::chrono::duration<Rep, Period> timeout) const { return link_status(std::chrono::duration_cast<std::chrono::milliseconds>(timeout));}
   
    std::error_code ether_address(rte_be32_t ip4addr, rte_ether_addr& mac_addr) noexcept;

    [[nodiscard]] std::shared_ptr<sinks_t> sinks() const noexcept  { return sinks_.load(std::memory_order_relaxed); }
    [[nodiscard]] std::uint16_t next_src_port() noexcept;
//TODO: most likely should be replaced with raw_tx_channel() -> interface;
    [[nodiscard]] tx_queue& raw_tx(queue_id_t hint = 0) noexcept;
    [[nodiscard]] dpdkx::tx_queue& service_tx(queue_id_t hint = 0) noexcept;
    [[nodiscard]] std::uint16_t process_rx_packets(queue_id_t queue_id, struct rte_mbuf** buffers, std::uint16_t n);
///////////
    //device(device const&) = delete;
    //device& operator=(device const&) = delete;
    [[nodiscard]] auto nrx() const noexcept { return rx_jobs_.size(); }
    [[nodiscard]] auto ntx() const noexcept { return tx_jobs_.size(); }
    [[nodiscard]] std::vector<job*> rx_jobs();
    [[nodiscard]] std::vector<job*> tx_jobs();
    [[nodiscard]] std::vector<job*> jobs();
    int start() noexcept;
    int stop() noexcept;
    std::error_code attach_rx(endpoint ep, std::shared_ptr<rx_channel> ch);
    std::error_code detach_rx(endpoint ep, std::shared_ptr<rx_channel> ch);
    std::error_code detach_rx(std::shared_ptr<rx_channel> ch);

    std::error_code join_mcast_group(rte_be32_t ip4addr) { return mcast_group(ip4addr, igm_record::change_to_exclude_mode); }
    std::error_code leave_mcast_group(rte_be32_t ip4addr) { return mcast_group(ip4addr, igm_record::change_to_include_mode); }
    constexpr auto has_timestamp(struct rte_mbuf const* mbuf) const noexcept { return (mbuf->ol_flags & dynfields_.timestamp.flag) != 0; }
    [[nodiscard]] rte_mbuf_timestamp_t* buffer_timestamp(struct rte_mbuf const* mbuf) const noexcept
    {
        return has_timestamp(mbuf)
                ? RTE_MBUF_DYNFIELD(mbuf, dynfields_.timestamp.offset, rte_mbuf_timestamp_t*)
                : nullptr;
    }

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
    std::atomic<std::shared_ptr<sinks_t>> sinks_;
    //std::atomic<std::shared_ptr<rte_lpm>> sinks_;
    std::atomic<std::uint16_t> next_src_port_;
};

[[nodiscard]] std::string svc_mempool_name(unsigned int socket_id);

inline rte_mbuf_timestamp_t timestamp(device const& device, struct rte_mbuf const* mbuf) noexcept {
    auto res = device.buffer_timestamp(mbuf);
    return res ? device.timestamp_fix(*res) : device.read_clock();
}

} // namespace dpdkx


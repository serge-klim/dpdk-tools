#include "loggers.hpp"
#include "utils.hpp"
#include "device.hpp"
#include "mbuf.hpp"
#include "detail/utils.hpp"
#include "utils/workarounds.hpp"
#include "error.hpp"
#include "rte_arp.h"
#include "rte_ethdev.h"
#include "rte_mbuf.h"
#include "rte_ip_frag.h"
#include "rte_ip.h"
#include <algorithm>
#include <string>
#include <ranges>
#include <cstdint>
#include <cassert>


namespace {
//static constexpr auto rx_ring_size = std::uint16_t{1024/*RX_RING_SIZE*/};

static constexpr auto RX_DESC_MAX    = unsigned(2048);
static constexpr auto TX_DESC_MAX    = unsigned(2048);
//static constexpr auto MAX_PKT_BURST  = unsigned(512);
//static constexpr auto DEF_PKT_BURST  = unsigned(32);
//static constexpr auto DEF_MBUF_CACHE = unsigned(250);

//https://github.com/DPDK/dpdk/blob/main/app/test-pmd/testpmd.h#L74
static constexpr auto mb_mempool_cache/*DEF_MBUF_CACHE*/ = unsigned{ 250 };

constexpr bool is_multicast_ipv4_addr(rte_be32_t ipv4_addr) noexcept {
    return ((rte_be_to_cpu_32(ipv4_addr) >> 24) & 0x000000FF) == 0xE0;
}

//consteval auto default_timeout() { return std::chrono::milliseconds{ 100 /* 100ms */ } };

auto find_sink(dpdkx::sinks_t const& sinks, rte_be16_t port) {
    return std::ranges::find_if(sinks, [port](auto const& ep) { return ep.first.second == port; });
}

}

dpdkx::device::tx_job::tx_job(device& device, queue_id_t queue_id, std::uint16_t burts_size)
    : device_{ device }
    , port_id_{ device.port_id()}
    , queue_id_{ queue_id }
    , burst_size_{ (std::min<std::uint16_t>)(burts_size, buffers_.size()) }
    , queue_{ device, queue_id}   
{
    assert(burts_size != 0);
}

dpdkx::job_state dpdkx::device::tx_job::process() {
    // if (auto link = device_.link_status(std::chrono::seconds{9}); !link || link->link_status == RTE_ETH_LINK_DOWN) {
    //     logging::logger<dpdkx::device>().info("tx_job::device {}[{}] is down", device_.id(), device_.port_id());
    //     return false;
    // }
    auto res = job_state::idling;
    if (likely(burst_size_ == n_ ||
               (n_ += /*rte_ring_dequeue_burst*/rte_ring_sc_dequeue_burst_elem(queue_.ring(), reinterpret_cast<void**>(buffers_.data() + n_), sizeof(void*), burst_size_ - n_, nullptr)) != 0)) {
        auto sent = rte_eth_tx_burst(port_id_, queue_id_, buffers_.data(), n_);
        if (likely(sent!=0)) {
            if (unlikely(n_ != sent)){
                auto begin = std::begin(buffers_);
                auto end = std::next(begin, n_);
                std::copy_backward(begin, std::next(begin, sent), end);
            }
            n_ -= sent;
        }
        res = job_state::busy;
    }
    return res;
}

dpdkx::device::rx_job::rx_job(device& device, queue_id_t queue_id, std::uint16_t burts_size, std::uint16_t reload_hint)
    : device_{ device }
    , port_id_{ device.port_id() }
    , queue_id_{ queue_id }
    , reload_checkpoint_ { reload_hint }
    , burst_size_{ (std::min)(burts_size, static_cast<std::uint16_t>(buffers_.size())) }
    , sinks_{device.sinks()}
{
    assert(burts_size != 0);
    //auto reload_point = (std::max)(128, burts_size * 10);
}


dpdkx::job_state dpdkx::device::rx_job::process() {

    auto find_rx_channel = [this] (endpoint const& ep)-> rx_channel* {
        if ((*sinks_)[last_].first.second != ep.second || (!(*sinks_)[last_].second && try_reload_sinks())) {
            auto i = find_sink(*sinks_, ep.second);
            last_ = (i == std::end(*sinks_))
                ? 0
                : std::distance(cbegin(*sinks_), i);
        }
        return (*sinks_)[last_].second.get();
    };

   
    try_reload_sinks();
    // if (auto link = device_.link_status(std::chrono::seconds{ 9 }); !link || link->link_status == RTE_ETH_LINK_DOWN) {
    //     logging::logger<dpdkx::device>().info("rx_job::device {}[{}] is down", device_.id(), device_.port_id());
    //     return false;
    // }
    auto res = job_state::idling;
    if (const auto n = rte_eth_rx_burst(port_id_, queue_id_, buffers_.data(), burst_size_)) {
        #pragma clang loop vectorize(enable) interleave(enable)
        for (auto i = decltype(n){0}; i != n - 1; ++i) {
            rte_prefetch0(rte_pktmbuf_mtod(buffers_[i + 1], void*));
            update_l2size(buffers_[i]);
        }
        update_l2size(buffers_[n-1]);
        //// 4 based on this:
        ////https://github.com/DPDK/dpdk/blob/main/app/test-pmd/macswap_sse.h
        //for (auto i = decltype(n){0};;) {
        //    switch (n) {
        //    default:
        //        assert(n != 0);
        //        rte_prefetch0(rte_pktmbuf_mtod(buffers_[i + 4], void*));
        //        rte_prefetch0(rte_pktmbuf_mtod(buffers_[i + 5], void*));
        //        rte_prefetch0(rte_pktmbuf_mtod(buffers_[i + 6], void*));
        //        rte_prefetch0(rte_pktmbuf_mtod(buffers_[i + 7], void*));
        //        [[fallthrough]];
        //    case 4:
        //        update_l2size(buffers_[i++]);
        //        [[fallthrough]];
        //    case 3:
        //        update_l2size(buffers_[i++]);
        //        [[fallthrough]];
        //    case 2:
        //        update_l2size(buffers_[i++]);
        //        [[fallthrough]];
        //    case 1:
        //        update_l2size(buffers_[i++]);
        //        continue;
        //    case 0:
        //        break;
        //    }
        //    break;
        //}

        rte_mbuf* notused[n];
        auto notused_n = decltype(n){0};
        auto left = decltype(n){0};
        for (auto i = decltype(n){0}; i != n; ++i) {
            switch (eth_proto/*update_l2size*/(buffers_[i])) {
                case 0:
                    logging::logger<dpdkx::device>().warn("broken packet, it seems {} ...", buffers_[i]->data_len);
                    notused[notused_n++] = buffers_[i]; //rte_pktmbuf_free(buffers_[i]);
                    break;
                case RTE_BE16(RTE_ETHER_TYPE_IPV4): {
                    auto ipv4hdr = rte_pktmbuf_mtod_offset(buffers_[i], rte_ipv4_hdr*, buffers_[i]->l2_len);
                    //assert(rte_ipv4_hdr_len(ipv4hdr) <= sizeof(*ipv4hdr));
                    auto const l3size = rte_ipv4_hdr_len(ipv4hdr)/*sizeof(*ipv4hdr)*/;
                    if (ipv4hdr->next_proto_id == IPPROTO_UDP) {
                        assert(!rte_ipv4_frag_pkt_is_fragmented(ipv4hdr));
                        auto udphdr = rte_pktmbuf_mtod_offset(buffers_[i], rte_udp_hdr*, buffers_[i]->l2_len + l3size);
                        if (buffers_[i]->data_len < buffers_[i]->l2_len + l3size + sizeof(struct rte_udp_hdr)
                            || buffers_[i]->data_len < buffers_[i]->l2_len + l3size + rte_be_to_cpu_16(udphdr->dgram_len)) {                        
                            ////l2size += sizeof(udphdr);
                            //auto dst_port = rte_be_to_cpu_16(udphdr->dst_port);
                            ////logging::logger<dpdkx::device>().info("broken UPD packet {} : {} size: {} + {} + {}", rte_be_to_cpu_16(udphdr->src_port), dst_port, int(buffers[i]->l2_len), int(l3size), rte_be_to_cpu_16(udphdr->dgram_len));
                            notused[notused_n++] = buffers_[i]; //rte_pktmbuf_free(buffers_[i]);
                            break;
                        }
                        if (auto ch = find_rx_channel(endpoint{ std::bit_cast<rte_be32_t>(ipv4hdr->dst_addr), std::bit_cast<decltype(udphdr->dst_port)>(udphdr->dst_port) })) {
                            buffers_[i]->l3_len = l3size/*sizeof(*ip_hdr)*/;
                            buffers_[i]->l4_len = sizeof(struct rte_udp_hdr);
                            if (!ch->enqueue(queue_id_, ipv4hdr, buffers_[i]))
                                notused[notused_n++] = buffers_[i]; //rte_pktmbuf_free(buffers_[i]);
                            res = job_state::busy;
                            break;
                        }
                    }
                    [[fallthrough]];
                }
                default:
                    buffers_[left++] = buffers_[i];
            }
        }
        if(notused_n)
            rte_pktmbuf_free_bulk(notused, notused_n);

        if (left != 0) {
            if(auto recycle = device_.process_rx_packets(queue_id_, buffers_.data(), left))
                rte_pktmbuf_free_bulk(buffers_.data(), recycle);
            if (n == left && left != burst_size_)
                checkpoint_ += 2*burst_size_;
        }
     } else 
        checkpoint_ += 2 * burst_size_;
     ++checkpoint_;
    return res;
}

//namespace {
//constexpr in_addr inaddr_any() noexcept {
//    auto res = in_addr{};
//    res.s_addr = INADDR_ANY;
//    return res;
//}
//}

dpdkx::device::device(config::device config)
    : svc_mempool_{ rte_mempool_lookup(svc_mempool_name(config.socket_id/*socket_id()*/).c_str()) }
    , rx_mbuf_pool_{nullptr, &rte_mempool_free}
    , config_{ std::move(config) }
    , sinks_{ std::make_shared<sinks_t>(sinks_t{sinks_t::value_type{}}) }
    , next_src_port_{config.next_src_port}
{
    if (config_.features[config::device::dev_clock]) {
        auto clock_hz = estimate_clock_freq(config_.port_id, clock_bswap_);
        if (clock_hz) {
            logging::logger<dpdkx::device>().info("estimated device {} - {} clock frequency {} hz / configured frequency {} hz - byte swap {}"
                , config.port_id, config.info.driver_name, clock_hz, config_.clock_hz, clock_bswap_);
            if (!config_.clock_hz)
                config_.clock_hz = clock_hz;
            //std::get<double>(clock_adjustment_) = double{ NS_PER_S } / config_.clock_hz;
        }
        else {
            logging::logger<dpdkx::device>().info("device {} - {} doesn't support clock, disabling...", config.port_id, config.info.driver_name);
            config_.features.reset(config::device::dev_clock);
            config_.clock_hz = 1; // to avoid division by zero
        }
    }

    if(!svc_mempool_)
        throw std::system_error{ dpdkx::make_error_code(ENOENT) , "unable to find " + svc_mempool_name(socket_id()) };

    if (config_.info.default_rxportconf.burst_size == 0)
        config_.info.default_rxportconf.burst_size = 32;
    if (config_.info.default_txportconf.burst_size == 0)
        config_.info.default_txportconf.burst_size = 32;

    rx_jobs_ = utils::workarounds::to<decltype(rx_jobs_)>(std::ranges::views::iota(queue_id_t{0}, queue_id_t{ config_.info.default_rxportconf.nb_queues })
        | std::ranges::views::transform([this](auto const& queue_ix) { return rx_job{ *this, queue_ix
                                                                        , config_.info.default_rxportconf.burst_size
                                                                        , config_.rx_reconfig_hint }; })
        )/*| std::ranges::to<decltype(rx_jobs_)>()*/;
    tx_jobs_ = utils::workarounds::to<decltype(tx_jobs_)>(std::ranges::views::iota(queue_id_t{ 0 }, queue_id_t{ config_.info.default_txportconf.nb_queues})
        | std::ranges::views::transform([this](auto const& queue_ix) { return tx_job{ *this, queue_ix, config_.info.default_txportconf.burst_size }; })
        )/*| std::ranges::to<decltype(tx_jobs_)>()*/;

    struct rte_eth_conf port_conf {};
    /*offloads_.rx =*/ port_conf.rxmode.offloads = rx_offloads();
    /*offloads_.tx =*/ port_conf.txmode.offloads = tx_offloads();
    // port_conf.rx_adv_conf.vmdq_rx_conf.enable_loop_back = 1;
    if (config_.info.default_rxportconf.nb_queues > 1) {
        if ((port_conf.rxmode.offloads & RTE_ETH_RX_OFFLOAD_RSS_HASH) != 0)
            port_conf.rxmode.mq_mode = RTE_ETH_MQ_RX_RSS;
    } else
        port_conf.rxmode.offloads &= ~RTE_ETH_RX_OFFLOAD_RSS_HASH;

    if (rte_mbuf_dyn_rx_timestamp_register(&dynfields_.timestamp.offset, &dynfields_.timestamp.flag) != 0) {
        dynfields_.timestamp.flag = 0;
        logging::logger<dpdkx::device>().warn("unable to register register timestamp field on port : {} : {}", port_id(), dpdkx::last_error().message());
    }

    //if (auto dynflag_offset = rte_mbuf_dynflag_lookup(RTE_MBUF_DYNFLAG_RX_TIMESTAMP_NAME, nullptr); 
    //    dynflag_offset >= 0 || rte_mbuf_dyn_rx_timestamp_register(&dynflag_offset, nullptr) == 0) {
    //    dynfields_.timestamp.offset = rte_mbuf_dynfield_lookup(RTE_MBUF_DYNFIELD_TIMESTAMP_NAME, nullptr);
    //    if(dynfields_.timestamp.offset >=0 )
    //        dynfields_.timestamp.flag = 1ULL << /*RTE_BIT64*/(dynflag_offset);
    //} else
    //    logging::logger<dpdkx::device>().warn("unable to register register timestamp field on port : {} : {}", port_id(), dpdkx::last_error().message());

    /* enable all supported rss offloads */
    //port_conf.rx_adv_conf.rss_conf.rss_hf = dev_info.flow_type_rss_offloads;
    //if (dev_info.hash_key_size) {
    //    port_conf.rx_adv_conf.rss_conf.rss_key = const_cast<uint8_t *>(_rss_key.data());
    //    port_conf.rx_adv_conf.rss_conf.rss_key_len = dev_info.hash_key_size;
    //}
    if (rte_eth_dev_configure(port_id(), config_.info.default_rxportconf.nb_queues, config_.info.default_txportconf.nb_queues, &port_conf) != 0)
        throw std::system_error{ dpdkx::last_error() , "rte_eth_dev_info_get failed" };

    if(config_.info.default_rxportconf.ring_size == 0)
       config_.info.default_rxportconf.ring_size = default_ring_size();
    if (config_.info.default_txportconf.ring_size == 0)
       config_.info.default_txportconf.ring_size = default_ring_size();

    if (rte_eth_dev_adjust_nb_rx_tx_desc(port_id(), &config_.info.default_rxportconf.ring_size, &config_.info.default_txportconf.ring_size) != 0)
        throw std::system_error{ dpdkx::last_error() , "rte_eth_dev_adjust_nb_rx_tx_desc failed" };

//#define NUM_MBUFS ((64*1024)-1)
//#define MBUF_CACHE_SIZE 128
//    static constexpr membuf_nuber_per_port = ((64 * 1024) - 1)

    const auto mbufs_pool_name = std::string{ "rx_" } + config_.id;
    const auto nb_mbuf_per_pool = RX_DESC_MAX + (config_.info.default_rxportconf.nb_queues/*nb_lcores*/ * mb_mempool_cache) + TX_DESC_MAX + max_pkt_burst;
    rx_mbuf_pool_ = dpdkx::make_scoped_mempool(mbufs_pool_name.c_str(), nb_mbuf_per_pool, mb_mempool_cache, 0, RTE_MBUF_DEFAULT_BUF_SIZE, static_cast<int>(socket_id())/*rte_socket_id()*/);

    auto rxq_config = config_.info.default_rxconf;
    rxq_config.offloads = port_conf.rxmode.offloads;
    for (auto q = decltype(config_.info.default_rxportconf.nb_queues){0}; q != config_.info.default_rxportconf.nb_queues; q++) {
        if(rte_eth_rx_queue_setup(port_id(), q, config_.info.default_rxportconf.ring_size, socket_id(), &rxq_config, rx_mbuf_pool_.get()) != 0)
            throw std::system_error{ dpdkx::last_error() , "rte_eth_rx_queue_setup failed" };
    }

    auto txq_config = config_.info.default_txconf;
    txq_config.offloads = port_conf.txmode.offloads;
    for (auto q = decltype(config_.info.default_txportconf.nb_queues){0}; q != config_.info.default_txportconf.nb_queues; q++) {
        if(rte_eth_tx_queue_setup(port_id(), q, config_.info.default_txportconf.ring_size, socket_id(), &txq_config) != 0)
            throw std::system_error{ dpdkx::last_error() , "rte_eth_tx_queue_setup failed" };
    }
    if(config_.svc_tx_n > config_.info.default_txportconf.nb_queues)
        config_.svc_tx_n = config_.info.default_txportconf.nb_queues;
}

dpdkx::device::~device() {
    // rte_eth_promiscuous_disable(port_id());
    stop();
    rte_eth_dev_close(port_id());
}

std::vector<dpdkx::job*> dpdkx::device::rx_jobs() {
    return utils::workarounds::to<std::vector<dpdkx::job*>>(
        rx_jobs_ | std::ranges::views::transform([](auto& job) {return static_cast<dpdkx::job*>(&job); })
    ) /*| std::ranges::to<std::vector<dpdkx::job*>>()*/;
}

std::vector<dpdkx::job*> dpdkx::device::tx_jobs() {
    return utils::workarounds::to<std::vector<dpdkx::job*>>(
        tx_jobs_ | std::ranges::views::transform([](auto& job) {return static_cast<dpdkx::job*>(&job); })
    ) /*| std::ranges::to<std::vector<dpdkx::job*>>()*/;
}

std::vector<dpdkx::job*> dpdkx::device::jobs() {
    auto res = std::vector<dpdkx::job*>{};
    res.reserve(rx_jobs_.size() + tx_jobs_.size());
    std::transform(begin(rx_jobs_), end(rx_jobs_), std::back_inserter(res), [](auto& job) {
        return &job;
        });
    std::transform(begin(tx_jobs_), end(tx_jobs_), std::back_inserter(res), [](auto& job) {
        return &job;
        });
    return res;
}

rte_mbuf_timestamp_t dpdkx::device::read_clock() const noexcept {
    auto ts = std::uint64_t{};
    return clock_enabled() && rte_eth_read_clock(port_id(), &ts) == 0
             ? timestamp_fix(clock_bswap() ? rte_bswap64(ts) : ts)
             : ns_timer();
}

std::optional<rte_eth_link> dpdkx::device::link_status() const noexcept{
    auto res = rte_eth_link {};
    return rte_eth_link_get_nowait(port_id(), &res) == 0
        ? std::make_optional(res) : std::nullopt;
}

std::optional<rte_eth_link> dpdkx::device::link_status(std::chrono::milliseconds timeout) const {
    auto [effective_timeout, retries] = retry(timeout);
    assert(retries != 0);
    do {
        auto link = link_status();
        if (link && link->link_status == RTE_ETH_LINK_UP)
            return link;
        rte_delay_ms(effective_timeout.count());
    } while (--retries);
    return {};
}

int dpdkx::device::start() noexcept {
    logging::logger<dpdkx::device>().info("starting up port {} ...", port_id());
    return rte_eth_dev_start(port_id());
}

int dpdkx::device::stop() noexcept {
    logging::logger<dpdkx::device>().info("shutting down {} ...", port_id());
    return rte_eth_dev_stop(port_id());
}

dpdkx::tx_queue& dpdkx::device::service_tx(queue_id_t hint /*= 0*/) noexcept {
    assert(config_.svc_tx_n <= tx_jobs_.size());
    auto ix = hint % config_.svc_tx_n + tx_jobs_.size() - config_.svc_tx_n;
    return tx_jobs_[ix].raw_tx();
}

dpdkx::tx_queue& dpdkx::device::raw_tx(queue_id_t hint /*= 0*/) noexcept {
    auto ix = hint% tx_jobs_.size();
    return tx_jobs_[ix].raw_tx();
}

std::error_code dpdkx::device::ether_address(rte_be32_t ip4addr, rte_ether_addr& mac_addr) noexcept {
    if ((ip4addr & 0x7f) == 0x7f) {
        rte_ether_addr_copy(&this->mac_addr(), &mac_addr);
        return {};
    }
    auto res = make_error_code(std::errc::not_supported); // -ENOTSUP
    arp4_.visit(ip4addr, [&res, &mac_addr](const auto& value) {
        if (value.second.timestamp != 0) {
            rte_ether_addr_copy(&value.second.mac_addr, &mac_addr);
            res = std::error_code{};
        } else
            res = make_error_code(std::errc::resource_unavailable_try_again);
    });
    if (res == make_error_code(std::errc::not_supported) && config_.features[config::device::arp_table]) {
        auto buffer = rte_pktmbuf_alloc(svc_mempool_);
        if (unlikely(!buffer))
            return make_error_code(std::errc::no_buffer_space);

        auto ethdr = make_ether(buffer, RTE_BE16(RTE_ETHER_TYPE_ARP), sizeof(rte_arp_hdr));
        ethdr->src_addr = this->mac_addr();
        std::fill_n(ethdr->dst_addr.addr_bytes, sizeof(ethdr->dst_addr.addr_bytes) / sizeof(ethdr->dst_addr.addr_bytes[0]), 0xff);
        auto arphdr = rte_pktmbuf_mtod_offset(buffer, rte_arp_hdr*, buffer->l2_len);
        arphdr->arp_hardware = rte_cpu_to_be_16(RTE_ARP_HRD_ETHER);
        arphdr->arp_protocol = rte_cpu_to_be_16(RTE_ETHER_TYPE_IPV4);
        arphdr->arp_hlen = RTE_ETHER_ADDR_LEN;
        arphdr->arp_plen = sizeof(uint32_t);
        arphdr->arp_opcode = rte_cpu_to_be_16(RTE_ARP_OP_REQUEST);
        rte_ether_addr_copy(&this->mac_addr(), &arphdr->arp_data.arp_sha);
        arphdr->arp_data.arp_sip = this->ip4addr();
        std::fill_n(arphdr->arp_data.arp_tha.addr_bytes, sizeof(arphdr->arp_data.arp_tha.addr_bytes) / sizeof(arphdr->arp_data.arp_tha.addr_bytes[0]), 0);
        arphdr->arp_data.arp_tip = ip4addr;
        if (likely(service_tx().enque(buffer)))
            arp4_.try_emplace(ip4addr, arp4rec{});
        return /*-EAGAIN*/ make_error_code(std::errc::resource_unavailable_try_again);
    }
    return res;
}

std::uint16_t dpdkx::device::next_src_port() noexcept {
    auto res = std::uint16_t{0};
    while ((res = next_src_port_++) < config_.next_src_port && next_src_port_.compare_exchange_weak(res, config_.next_src_port, std::memory_order_release, std::memory_order_relaxed)) { }
    return res;
}

std::error_code dpdkx::device::attach_rx(endpoint ep, std::shared_ptr<rx_channel> ch) {
    auto updated = false;
    do {
        auto active = sinks_.load();
        auto sinks = std::make_shared<sinks_t>(*active);
        if (!sinks->emplace_back(std::move(ep), std::move(ch)).second)
            return make_error_code(std::errc::address_in_use);
        updated = sinks_.compare_exchange_strong(active, std::move(sinks));
    } while(!updated);
    return RTE_IS_IPV4_MCAST(rte_be_to_cpu_32(ep.first))
                    ? join_mcast(ep.first)
                    : std::error_code{};
}

std::error_code dpdkx::device::detach_rx(endpoint ep, std::shared_ptr<rx_channel> ch) {
    auto attempt = 0;
    auto updated = false;
    do {
        auto active = sinks_.load();
        auto sinks = active;
        auto sink = find_sink(*sinks, ep.second);
        if (sink == end(*sinks) || sink->second != ch) {
            return attempt == 0 
                ? make_error_code(std::errc::invalid_argument)
                : std::error_code{};
        }
        sinks->erase(sink);
        ++attempt;
        updated = sinks_.compare_exchange_strong(active, std::move(sinks));
    } while (!updated);
    return RTE_IS_IPV4_MCAST(rte_be_to_cpu_32(ep.first))
                    ? leave_mcast(ep.first)
                    : std::error_code{};
}

std::error_code dpdkx::device::join_mcast(rte_be32_t ip4addr) {
    {
        auto guard = std::lock_guard<std::mutex>{ mcast_guard_ };
        auto [i, emplaced] = joined_mcasts_.emplace(ip4addr, 1);
        if (!emplaced) {
            ++i->second;
            return make_error_code(std::errc::address_in_use);
        }
    }
    return join_mcast_group(ip4addr);
}

std::error_code dpdkx::device::leave_mcast(rte_be32_t ip4addr) {
    {
        auto guard = std::lock_guard{ mcast_guard_ };
        auto i = joined_mcasts_.find(ip4addr);
        if (i == end(joined_mcasts_))
            return make_error_code(std::errc::bad_address);
        if (--i->second == 0)
            joined_mcasts_.erase(i);
    }
    return leave_mcast_group(ip4addr);
}

std::uint16_t dpdkx::device::process_rx_packets(queue_id_t queue_id, struct rte_mbuf** buffers, std::uint16_t n) {
    auto used = decltype(n){0};
    for (auto i = decltype(n){0}; i != n; ++i) {
        auto ether_hdr = rte_pktmbuf_mtod(buffers[i], struct rte_ether_hdr*);
        switch (auto const proto = eth_proto(buffers[i])) {
            case RTE_BE16(RTE_ETHER_TYPE_IPV4): {
                auto ipv4hdr = rte_pktmbuf_mtod_offset(buffers[i], rte_ipv4_hdr*, buffers[i]->l2_len);
                //assert(rte_ipv4_hdr_len(ipv4hdr) <= sizeof(*ipv4hdr));
                auto const l3size = rte_ipv4_hdr_len(ipv4hdr)/*sizeof(*ipv4hdr)*/;
                if (buffers[i]->data_len < buffers[i]->l2_len + l3size){
                    logging::logger<dpdkx::device>().warn("IPV4 : data len:{} < {}", buffers[i]->data_len, buffers[i]->l2_len + l3size);
                    break;
                }
                switch (/*auto proto_id =*/ ipv4hdr->next_proto_id) {
                    //case IPPROTO_IGMP:{
                    //    auto udphdr = rte_pktmbuf_mtod_offset(buffers[i], rte_udp_hdr*, l2size);
                    //    //l2size += sizeof(udphdr);
                    //    std::clog << "IGMP size : " << l2size << '+' << rte_be_to_cpu_16(udphdr->dgram_len) << std::endl;
                    //    break;
                    //}
                    case IPPROTO_ICMP: {
                        auto icmp_hdr = rte_pktmbuf_mtod_offset(buffers[i], rte_icmp_hdr*, buffers[i]->l2_len + l3size);
                        //if (config_.features[config::device::arp_table])
                        //    arp4_.emplace(ipv4hdr->src_addr, ether_hdr->src_addr);
                        if (icmp_hdr->icmp_type == RTE_IP_ICMP_ECHO_REQUEST && icmp_hdr->icmp_code == 0
                            && config_.features[config::device::icmp]
                            && buffers[i]->data_len >= buffers[i]->l2_len + l3size + sizeof(struct rte_icmp_hdr) //make sure there is enough data to process in the buffer 
                            )
                        {
                            //shamelessly stolen:
                            //https://github.com/DPDK/dpdk/blob/main/app/test-pmd/icmpecho.c#L430
                            rte_ether_addr_copy(&ether_hdr->src_addr, &ether_hdr->dst_addr);
                            rte_ether_addr_copy(&mac_addr(), &ether_hdr->src_addr);
                            if (is_multicast_ipv4_addr(ipv4hdr->dst_addr)) {
                                auto ip_src = rte_be_to_cpu_32(ipv4hdr->src_addr);
                                ip_src =(ip_src & 0x00000003) == 1
                                    ? (ip_src & 0xFFFFFFFC) | 0x00000002
                                    : (ip_src & 0xFFFFFFFC) | 0x00000001;
                                ipv4hdr->dst_addr = ipv4hdr->src_addr;
                                ipv4hdr->src_addr = rte_cpu_to_be_32(ip_src);
                                ipv4hdr->hdr_checksum = 0;
                                //buffers[i]->l2_len = l2size/*sizeof(*eth_hdr)*/;
                                buffers[i]->l3_len = l3size/*sizeof(*ip_hdr)*/;
                                if (tx_offloads() & RTE_ETH_TX_OFFLOAD_IPV4_CKSUM)
                                    buffers[i]->ol_flags = RTE_MBUF_F_TX_IPV4 | RTE_MBUF_F_TX_IP_CKSUM;
                                else {
                                    buffers[i]->ol_flags = 0;
                                    ipv4hdr->hdr_checksum = rte_ipv4_cksum(ipv4hdr);
                                }
                            }
                            else {
                                ipv4hdr->dst_addr = ipv4hdr->src_addr;
                                ipv4hdr->src_addr = ip4addr();
                            }
                            icmp_hdr->icmp_type = RTE_IP_ICMP_ECHO_REPLY;
                            auto cksum = static_cast<std::uint32_t>(~icmp_hdr->icmp_cksum & 0xffff);
                            cksum += ~RTE_BE16(RTE_IP_ICMP_ECHO_REQUEST << 8) & 0xffff;
                            cksum += RTE_BE16(RTE_IP_ICMP_ECHO_REPLY << 8);
                            cksum = (cksum & 0xffff) + (cksum >> 16);
                            cksum = (cksum & 0xffff) + (cksum >> 16);
                            icmp_hdr->icmp_cksum = ~cksum;
                            if (service_tx(queue_id).enque(buffers[i]))
                                continue;
                        }
                        break;
                    }
                    default:
                        //logging::logger<dpdkx::device>().debug("next_proto_id : {} header size : {}  + ...",  int(proto_id), buffers[i]->l2_len + l3size);
                        break;
                }
                break;
            }
            case RTE_BE16(RTE_ETHER_TYPE_ARP): {
                if (buffers[i]->data_len < buffers[i]->l2_len + sizeof(struct rte_arp_hdr))
                    break;
                auto arphdr = rte_pktmbuf_mtod_offset(buffers[i], struct rte_arp_hdr*, buffers[i]->l2_len);
                if (arphdr->arp_hardware == RTE_BE16(RTE_ARP_HRD_ETHER) && arphdr->arp_protocol == RTE_BE16(RTE_ETHER_TYPE_IPV4)) {
                    switch (arphdr->arp_opcode) {
                        case RTE_BE16(RTE_ARP_OP_REQUEST):
                            logging::logger<dpdkx::device>().info("ARP request");
                            if (config_.features[config::device::arp_table])
                                arp4_.insert_or_assign(arphdr->arp_data.arp_sip, arp4rec{ arphdr->arp_data.arp_sha, read_clock()});
                            if(ip4addr() == arphdr->arp_data.arp_tip) {
                                arphdr->arp_opcode = RTE_BE16(RTE_ARP_OP_REPLY);
                                rte_ether_addr_copy(&ether_hdr->src_addr, &ether_hdr->dst_addr);
                                rte_ether_addr_copy(&mac_addr(), &ether_hdr->src_addr);
                                rte_ether_addr_copy(&arphdr->arp_data.arp_sha, &arphdr->arp_data.arp_tha);
                                arphdr->arp_data.arp_tip = arphdr->arp_data.arp_sip;
                                arphdr->arp_data.arp_sip = ip4addr();
                                rte_ether_addr_copy(&mac_addr(), &arphdr->arp_data.arp_sha);
                                if(service_tx(queue_id).enque(buffers[i]))
                                    continue;
                                //TODO:deal with dropped packets
                            }
                            break;
                        case RTE_BE16(RTE_ARP_OP_REPLY):
                            logging::logger<dpdkx::device>().info("ARP  reply ");
                            if (config_.features[config::device::arp_table])
                                arp4_.insert_or_assign(arphdr->arp_data.arp_sip, arp4rec{ arphdr->arp_data.arp_sha, read_clock() });
                            break;
                    }
                }
                else
                    logging::logger<dpdkx::device>().info("ARP : {}", proto);
                break;
            }
            case RTE_BE16(RTE_ETHER_TYPE_RARP):
                logging::logger<dpdkx::device>().info("RARP : {}", proto);
                break;
            case RTE_BE16(RTE_ETHER_TYPE_IPV6):
                logging::logger<dpdkx::device>().info("IPV6 : {}", proto);
                break;
            default:
                logging::logger<dpdkx::device>().info("::: {}", proto);
        }
        //rte_pktmbuf_free(buffers[i]);
        buffers[used++] = buffers[i];
    }
    return used;
}

// std::uint16_t dpdkx::device::rx_callback(port_id_t port_id, queue_id_t queue, struct rte_mbuf* pkts[], std::uint16_t nb_pkts, std::uint16_t max_pkts, void* user_param) {
//     return nb_pkts;
// }

std::string dpdkx::svc_mempool_name(unsigned int socket_id) {
    return "svc_mempool_" + std::to_string(socket_id);
}


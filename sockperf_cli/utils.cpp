#include "loggers.hpp"
#include "utils.hpp"
#include "dpdkx/mbuf.hpp"
#include "dpdkx/device.hpp"
#include "dpdkx/error.hpp"
#include "dpdkx/utils.hpp"
#include "sockperf/sockperf.hpp"
#include "sockperf/x.hpp"
#include <tuple>
#include <utility>
#include <chrono>
#include <ranges>
#include <cassert>



std::pair<std::size_t, std::uint64_t> configure_sockperf_packet_pool(dpdkx::device& device, rte_mempool* packet_pool, std::size_t payload_size, std::pair<rte_be32_t, rte_ether_addr> const& dest, rte_be16_t dst_port, std::uint8_t time_to_live /*= 64*/) {
    assert(dst_port != 0);

    auto const buffer_size = RTE_PKTMBUF_HEADROOM + sizeof(rte_ether_hdr) + sizeof(rte_ipv4_hdr) + sizeof(struct rte_udp_hdr) + payload_size;
    auto data = std::vector<char>(buffer_size);
    auto fake_buffer = rte_mbuf{};
    fake_buffer.buf_addr = data.data();
    fake_buffer.data_off = 0;
    auto udphdr = dpdkx::make_udp(&fake_buffer, payload_size);
    auto ethdr = rte_pktmbuf_mtod(&fake_buffer, rte_ether_hdr*);
    assert(ethdr->ether_type == rte_cpu_to_be_16(RTE_ETHER_TYPE_IPV4));
    auto ip4hdr = rte_pktmbuf_mtod_offset(&fake_buffer, rte_ipv4_hdr*, fake_buffer.l2_len);
    assert(udphdr == rte_pktmbuf_mtod_offset(&fake_buffer, rte_udp_hdr*, fake_buffer.l2_len + fake_buffer.l3_len));
    rte_ether_addr_copy(&device.mac_addr(), &ethdr->src_addr);
    rte_ether_addr_copy(&std::get<rte_ether_addr>(dest), &ethdr->dst_addr);
    ip4hdr->src_addr = device.ip4addr();
    ip4hdr->dst_addr = std::get<rte_be32_t>(dest);
    ip4hdr->time_to_live = time_to_live;
    udphdr->dst_port = dst_port;
    udphdr->src_port = rte_cpu_to_be_16(device.next_src_port());
    auto ol_flags = std::uint64_t{ 0 };
    if (device.tx_offloads() & RTE_ETH_TX_OFFLOAD_IPV4_CKSUM) {
        ol_flags |= RTE_MBUF_F_TX_IPV4 | RTE_MBUF_F_TX_IP_CKSUM;
    }
    else {
        ip4hdr->hdr_checksum = rte_ipv4_cksum(ip4hdr);
    }

    assert(udphdr->dgram_cksum == 0);
    if (device.tx_offloads() & RTE_ETH_TX_OFFLOAD_UDP_CKSUM) {
        ol_flags |= RTE_MBUF_F_TX_UDP_CKSUM;
        udphdr->dgram_cksum = rte_ipv4_phdr_cksum(ip4hdr, ol_flags);
    }
    else
        udphdr->dgram_cksum = rte_ipv4_udptcp_cksum_mbuf(&fake_buffer, ip4hdr, fake_buffer.l2_len + fake_buffer.l3_len) // multi segment
        /*rte_ipv4_udptcp_cksum(ip4hdr, udphdr)*/
        ;

    assert(fake_buffer.l2_len == sizeof(rte_ether_hdr));
    assert(fake_buffer.l3_len == /*rte_ipv4_hdr_len(ip4hdr)*/sizeof(rte_ipv4_hdr));
    assert(fake_buffer.l4_len == sizeof(struct rte_udp_hdr));

    auto payload = rte_pktmbuf_mtod_offset(&fake_buffer, char*, fake_buffer.l2_len + fake_buffer.l3_len + fake_buffer.l4_len);
    sockperf::seqn(0/*segn++*/, payload, payload_size);
    sockperf::message_type(sockperf::type::Ping, payload, payload_size);
    sockperf::message_size(payload_size, payload, payload_size);
    auto const packet_size = fake_buffer.l2_len + fake_buffer.l3_len + fake_buffer.l4_len + payload_size;

    assert(rte_mempool_in_use_count(packet_pool) == 0);

    auto const pool_size = static_cast<std::size_t>(rte_mempool_avail_count(packet_pool));
    auto buffers = std::vector<rte_mbuf*>(pool_size);
    if(rte_pktmbuf_alloc_bulk(packet_pool,buffers.data(), pool_size) != 0)
        throw std::system_error{ dpdkx::last_error() , "rte_pktmbuf_alloc_bulk failed" };
    for (auto& buffer : buffers) 
        std::memcpy(rte_pktmbuf_mtod(buffer, char*), rte_pktmbuf_mtod(&fake_buffer, char*), packet_size);

    rte_pktmbuf_free_bulk(buffers.data(), pool_size);
    return { pool_size, ol_flags };
}

namespace {

constexpr auto packet_size(std::size_t payload_size) noexcept {
    return sizeof(rte_ether_hdr)
    + /*rte_ipv4_hdr_len(ip4hdr)*/sizeof(rte_ipv4_hdr)
    + sizeof(struct rte_udp_hdr)
    + payload_size;
}

void prepare_packet_headers(rte_mbuf* buffer, std::uint64_t ol_flags, std::size_t payload_size) {
    buffer->l2_len = sizeof(rte_ether_hdr);
    buffer->l3_len = /*rte_ipv4_hdr_len(ip4hdr)*/sizeof(rte_ipv4_hdr);
    buffer->l4_len = sizeof(struct rte_udp_hdr);
    buffer->data_len = buffer->pkt_len = buffer->l2_len + buffer->l3_len + buffer->l4_len + payload_size;
    buffer->ol_flags = ol_flags;
    assert(packet_size(payload_size) == buffer->data_len);
    //auto ethdr = rte_pktmbuf_mtod(buffer, rte_ether_hdr*);
}

[[nodiscard]] constexpr auto ip4_header(rte_mbuf* buffer) noexcept {
    return rte_pktmbuf_mtod_offset(buffer, rte_ipv4_hdr*, buffer->l2_len);
}

[[nodiscard]] constexpr auto udp_header(rte_mbuf* buffer) noexcept {
    return rte_pktmbuf_mtod_offset(buffer, rte_udp_hdr*, buffer->l2_len + buffer->l3_len);
}

[[nodiscard]] constexpr auto sockperf_payloadf(rte_mbuf* buffer) noexcept {
    return rte_pktmbuf_mtod_offset(buffer, char*, buffer->l2_len + buffer->l3_len + buffer->l4_len);
}

void timestamp_packet(dpdkx::device const& /*device*/, rte_mbuf* buffer, std::uint64_t ol_flags, std::size_t payload_size) {
    auto payload = sockperf_payloadf(buffer);
    if (/*auto sockperf_payload_size = */sockperf::payload_size(payload, payload_size) != 0) {
        assert(sockperf::payload_size(payload, payload_size) >= sizeof(rte_mbuf_timestamp_t));
        auto timestamp = boost::endian::native_to_big(dpdkx::ns_timer()/*device.read_clock()*/);
        std::memcpy(sockperf::payload(payload, payload_size), &timestamp, sizeof(timestamp));
    }
    auto ip4hdr = ip4_header(buffer);
    auto udphdr = udp_header(buffer);
    udphdr->dgram_cksum = 0;
    udphdr->dgram_cksum = (buffer->ol_flags & RTE_MBUF_F_TX_UDP_CKSUM) == RTE_MBUF_F_TX_UDP_CKSUM
        ? rte_ipv4_phdr_cksum(ip4hdr, ol_flags)
        : rte_ipv4_udptcp_cksum(ip4hdr, udphdr); //  rte_ipv4_udptcp_cksum_mbuf(buffer, ip4hdr, buffer->l2_len + buffer->l3_len); // multi segment
}

void prepare_packet(dpdkx::device const& device, sockperf::seqn_t seqn, sockperf::type type, rte_mbuf* buffer, std::uint64_t ol_flags, std::size_t payload_size) {
    prepare_packet_headers(buffer, ol_flags, payload_size);
    auto payload = sockperf_payloadf(buffer);
    sockperf::seqn(seqn, payload, payload_size);
    sockperf::message_type(type/*sockperf::type::Ping*/, payload, payload_size);
    timestamp_packet(device, buffer, ol_flags, payload_size);
}

}

latency_test_job::latency_test_job(sockperf_channel& rx_channel, dpdkx::queue_id_t queue_id, dpdkx::shared_mempool packet_pool, std::size_t packets2send, std::uint64_t ol_flags, std::size_t payload_size)
    : rx_channel_{ rx_channel }
    , queue_id_{ queue_id }
    , packet_pool_{ std::move(packet_pool) }
    , packets2send_{ packets2send }
    , ol_flags_{ ol_flags }
    , payload_size_{ payload_size }
{}

dpdkx::job_state latency_test_job::process() {
    auto seqn = decltype( packets2send_){1};
    auto const& device = rx_channel_.device();
    auto port_id = device.port_id();
    auto const use_dev_clock = device.clock_enabled()
                                && (device.rx_offloads() & RTE_ETH_RX_OFFLOAD_TIMESTAMP) == RTE_ETH_RX_OFFLOAD_TIMESTAMP;
//    auto buffer = std::unique_ptr<rte_mbuf, decltype(&rte_pktmbuf_free)>{ nullptr, &rte_pktmbuf_free };
    auto [effective_timeout, retries] = dpdkx::retry(std::chrono::milliseconds{ 500 });
    while (seqn < packets2send_ && !dpdkx::jobs_suspended()) {
        if (auto buffer = rte_pktmbuf_alloc(packet_pool_.get())) {
            prepare_packet(device, sockperf::x::warmup(seqn), sockperf::type::/*Warmup*/Ping, buffer, ol_flags_, payload_size_);
            auto sent_at = use_dev_clock ? device.read_clock() : dpdkx::ns_timer();
            if (likely(rte_eth_tx_burst(port_id, queue_id_, &buffer, 1) != 0)) {
                for (auto i = retries;!dpdkx::jobs_suspended();) {
                    if (auto msg = rx_channel_.last_warmup_message()) {
                        if (msg->seqn == sockperf::x::warmup(seqn)) {
                            auto received_at = use_dev_clock ? device.timestamp_fix(msg->received_ts) : msg->enqueued_ts;
                            BOOST_LOG_SEV(log::get(), boost::log::trivial::info) << " rtt = " 
                                                       << std::chrono::duration_cast<std::chrono::duration<double,std::micro>>(std::chrono::nanoseconds{ received_at - sent_at }).count() << " us.";
                            break;                            
                        }
                    }
                    if (i-- == 0) {
                        BOOST_LOG_SEV(log::get(), boost::log::trivial::warning) << "no reply, timed out";
                        break;
                    }
                    rte_delay_ms(effective_timeout.count());
                }
                ++seqn;
            }
            else
                rte_pktmbuf_free(buffer);
        }
    }
    return dpdkx::job_state::done;
}

tx_job::tx_job(dpdkx::device& device, dpdkx::queue_id_t queue_id, dpdkx::shared_mempool packet_pool, std::size_t packets2send, std::uint64_t ol_flags, std::size_t payload_size)
    : device_{ device }
    , queue_id_{ queue_id }
    , packet_pool_{ std::move(packet_pool) }
    , packets2send_{ packets2send }
    , ol_flags_{ ol_flags }
    , payload_size_{ payload_size }
{}

dpdkx::job_state tx_job::process() {
    auto seqn = sockperf::seqn_t{ 0 };
    auto const burst_size = device_.dev_info().default_txportconf.burst_size == 0 ? std::uint16_t{ 32 } : device_.dev_info().default_txportconf.burst_size;
    auto buffers = std::vector<rte_mbuf*>(burst_size);
    auto offset = std::size_t{0};
    //auto start_ns_timer = dpdkx::ns_timer();
    //auto start = std::make_pair(std::chrono::high_resolution_clock::now(), device_.read_clock());
    auto start = device_.read_clock();
    while(seqn <  packets2send_ /*+ packets2send_ * .05*/) {
        assert(burst_size != 0);              
        if (likely(rte_pktmbuf_alloc_bulk(packet_pool_.get(), buffers.data() + offset, burst_size - offset) == 0)) {
            for (auto buffer : std::ranges::subrange(std::next(begin(buffers), offset), end(buffers)))
                prepare_packet(device_, sockperf::x::make_seqn(queue_id_,++seqn), sockperf::type::Ping, buffer, ol_flags_, payload_size_);
            for(;;){
               auto sent = rte_eth_tx_burst(device_.port_id(), queue_id_, buffers.data(), burst_size);
               if(likely(sent!=0)){
                   offset = burst_size - sent;
                   if (unlikely(offset != 0)) {
                       //std::copy_backward(std::next(begin(buffers), sent), end(buffers), std::next(begin(buffers), offset));
                       //check if vectorized
                       for (auto ix : std::ranges::iota_view{ static_cast<decltype(sent)>(0), sent}) {
                           timestamp_packet(device_, buffers[burst_size - ix - 1], ol_flags_, payload_size_);
                           buffers[sent - ix - 1] = buffers[burst_size - ix - 1];
                       }
                   }
                   break;
               }
               //std::this_thread::yield();
            }                            
        } else {
             BOOST_LOG_SEV(log::get(), boost::log::trivial::warning) << queue_id_ << " : not enough entries in the mempool after sending " << seqn << " packets...";
            // //https://stackoverflow.com/questions/69169224/rte-eth-tx-burst-descriptor-mbuf-management-guarantees-vs-free-thresholds
            // rte_eth_tx_done_cleanup(device_.port_id(), queue_id_, (std::max)(burst_size * 4,32));
        }       
    }
    //auto ns_timer_dur = dpdkx::ns_timer() - start_ns_timer;
    //auto duration = std::chrono::duration_cast<std::chrono::nanoseconds>(std::chrono::high_resolution_clock::now() - std::get<std::chrono::high_resolution_clock::time_point>(start));
    //auto dev_dur = device_.read_clock() - std::get<rte_mbuf_timestamp_t>(start);
    auto dev_dur = device_.read_clock() - start;
    auto const bytes_sent = seqn * packet_size(payload_size_);
    BOOST_LOG_SEV(log::get(), boost::log::trivial::info) << "tx queue: " << queue_id_ << ":packet size: "<< packet_size(payload_size_) << " bytes / payload size: " << payload_size_ <<" bytes \n\t\t"
    //BOOST_LOG_SEV(log::get(), boost::log::trivial::info) << "tx queue: " << queue_id_ << " : " << bytes_sent <<" bytes in "<< seqn << " packets has been sent in " << std::chrono::duration_cast<std::chrono::microseconds>(duration).count() << " us - "
    //                        << std::chrono::duration_cast<std::chrono::microseconds>(std::chrono::nanoseconds{dev_dur}).count() << " us : " << dev_dur << " ticks " << ns_timer_dur <<" ns."
    //    << "\n\t\t~" << static_cast<double>(bytes_sent * 8) / duration.count()
    //                        <<"/" <<static_cast<double>(bytes_sent * 8) / dev_dur << " Gbps " << seqn * std::nano::den / (duration.count() == 0 ? 1 : duration.count()) << " packets per second";
    /*BOOST_LOG_SEV(log::get(), boost::log::trivial::info) << "tx queue:" << queue_id_ << " : "*/ << bytes_sent <<" bytes in "<< seqn << " packets has been sent in " 
                                                          << std::chrono::duration_cast<std::chrono::microseconds>(std::chrono::nanoseconds{dev_dur}).count() << " us : " << dev_dur << " ticks / "
         << seqn * std::nano::den / (dev_dur == 0 ? 1 : dev_dur) << " packets per second"
        " (~" <<static_cast<double>(bytes_sent * 8) / dev_dur << " Gbps)"
        ;
    return dpdkx::job_state::done;
}

std::error_code tx_job::warmup(sockperf_channel& channel, std::vector<dpdkx::job*>& jobs, std::chrono::milliseconds timeout) {
    auto seqn = sockperf::seqn_t{ 0 };
    auto& q = device_.raw_tx()/* service_tx()*/;
    auto buffer = std::unique_ptr<rte_mbuf, decltype(&rte_pktmbuf_free)>{nullptr, &rte_pktmbuf_free};

    auto start = std::chrono::system_clock::now();
    for (auto i = 0; (!channel.last_warmup_message() || seqn < 16) && !dpdkx::jobs_suspended(); ++i) {
        if ((i & 0xf) == 0) {
            if (!buffer) {
                buffer.reset(rte_pktmbuf_alloc(packet_pool_.get()));
                if (buffer)
                    prepare_packet(device_, sockperf::x::warmup(++seqn), sockperf::type::/*Warmup*/Ping, buffer.get(), ol_flags_, payload_size_);
            }
            if(buffer)
            {                
                if (q.enque(buffer.get()))
                    buffer.release();
            }
        }
        if(jobs.empty())
            return make_error_code(std::errc::owner_dead);
        if(std::chrono::system_clock::now() - start > timeout)
            return make_error_code(std::errc::timed_out);
        dpdkx::run_jobs_once(jobs);
    }
    start = std::chrono::system_clock::now();
    while(!dpdkx::jobs_suspended() && channel.last_warmup_message()->seqn != sockperf::x::warmup(seqn) && std::chrono::system_clock::now() - start < timeout)
        dpdkx::run_jobs_once(jobs);
    return {};
}

sockperf_channel::sockperf_channel(use_make_rx_channel, dpdkx::device& device, sockaddr_in const& addr, std::size_t packets2send, bool detailed_stats /*= false*/)
    : dpdkx::rx_channel{ dpdkx::rx_channel::use_make_rx_channel{}, device, addr }
    , packets2send_{ packets2send * device.ntx()}
    , stats_{ detailed_stats ? packets2send_ : std::size_t{2} , static_cast<std::uint8_t>(device.ntx()) }
{
}

bool sockperf_channel::enqueue(dpdkx::queue_id_t /*queue_id*/, rte_ipv4_hdr* /*ipv4hdr*/, rte_mbuf* buffer)
{ 
    auto const enqueued = dpdkx::ns_timer(); 
    //auto timestamp = dpdkx::timestamp(device(), buffer) /*+ 2574114773570051072LL - 1500000LL*/;  // increases missed packets from 0-0.2% to over 90% on small packets and around 80 on packets 1024+
    auto timestamp = device().buffer_timestamp(buffer);    
    auto const msg_header_size = buffer->l2_len + buffer->l3_len + buffer->l4_len;
    if (buffer->data_len >= msg_header_size + sockperf::header_size()) {
        auto payload = rte_pktmbuf_mtod_offset(buffer, char*, msg_header_size);
        switch (auto type = sockperf::message_type(payload, buffer->data_len - msg_header_size)) {
            case sockperf::type::Ping:
                [[fallthrough]];
            case sockperf::type::Pong: {
                auto seqn = sockperf::seqn(payload, buffer->data_len - msg_header_size);
                if (unlikely(sockperf::x::is_warmup(seqn))) {
                    auto received_msg_info = std::make_shared<message_info>();
                    received_msg_info->seqn = seqn;
                    received_msg_info->received_ts = timestamp ? *timestamp : enqueued;
                    received_msg_info->enqueued_ts = enqueued;
                    last_warmup_message_ = received_msg_info;
                    break;
                }
                if (auto payload_size = sockperf::payload_size(payload, buffer->data_len - msg_header_size); payload_size >= sizeof(rte_mbuf_timestamp_t)) {
                    if (stats_.requested_slots() == packets2send_) {
                        dpdkx::stop_jobs();
                        break;
                    }
                    auto& info = stats_.slot(0);
                    info.seqn = seqn;
                    static_assert(std::is_same_v<decltype(info.sent), rte_mbuf_timestamp_t>, "oops! packet sent timestamp type needs to be adjusted");
                    std::memcpy(&info.sent, sockperf::payload(payload, buffer->data_len - msg_header_size), sizeof(info.sent));
                    info.received = timestamp ? device().timestamp_fix(*timestamp) : enqueued;
                    info.enqueued_rx = enqueued;
                    info.size = buffer->data_len;
                }
                break;
            }
        }
    }
   return false;
}

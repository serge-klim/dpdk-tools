#include "loggers.hpp"
#include "utils.hpp"
#include "sockperf/x.hpp"
#include "dpdkx/io.hpp"
#include "dpdkx/utils.hpp"
#include "dpdkx/udptx_channel.hpp"
#include "dpdkx/jobs.hpp"
#include "dpdkx/error.hpp"
#include "dpdkx/device.hpp"
#include "dpdkx/mempool.hpp"
#include "dpdkx/loggers.hpp"
#include "dpdkx/config/socket.hpp"
#include "dpdkx/config/program_options.hpp"
#include "dpdkx/config/io.hpp"
#include "dpdkx/netinet_in.hpp"
#include "utils/program_options/net.hpp"
#include "utils/workarounds.hpp"
#include <boost/program_options.hpp>
#include <functional>
#include <set>
#include <list>
#include <ranges>
#include <cassert>
#include <csignal>


void run(boost::program_options::variables_map const& options) {
	auto destination = options["destination"].as<net::endpoint>();

    sockaddr_in dest_addr;
    dest_addr.sin_family = AF_INET;
    if (inet_pton(AF_INET, destination.host.c_str(), &dest_addr.sin_addr) != 1) {
        BOOST_LOG_SEV(log::get(), boost::log::trivial::error) << "invalid ip4 address : " << destination.host;
        return;
    }
    dest_addr.sin_port = htons(destination.port);

    auto const& nics_info = dpdkx::v0::config::get_nic_info(options);
    for (auto const& info : nics_info) {
        auto txt = info.name;
        if (!info.mac.empty()) {
            auto mac = std::array<char, RTE_ETHER_ADDR_FMT_SIZE>{};
            rte_ether_format_addr(mac.data(), mac.size(), reinterpret_cast<rte_ether_addr const*>(info.mac.data()));
            txt += "\n\t";
            txt += mac.data();
        }
        if (!info.ipv4.empty()) {
            txt += "\n\t";
            txt += inet_ntoa(info.ipv4.front());
        }
        BOOST_LOG_SEV(log::get(), boost::log::trivial::debug) << txt;
    }

    if (auto logtype = rte_log_register("sockperf"); logtype >= 0)
        rte_log_set_level(logtype, /*RTE_LOG_DEBUG*/RTE_LOG_MAX);
    else
        BOOST_LOG_SEV(log::get(), boost::log::trivial::info) << "Cannot register log type : " << logtype;

    auto sentry = dpdkx::eal_init(dpdkx::config::eal_option("sockperf", options));

    auto devices = dpdkx::config::devices_configuration(nics_info, options);
    if (devices.empty())
        throw std::runtime_error{ "no suitable device found" };

    auto& device_config = devices.front();

    auto const& sockets_config = dpdkx::config::socket_configuration();
    if (sockets_config.sockets.empty())
        throw std::runtime_error{ "no sockets available" };

    auto socket_config = std::ranges::find(sockets_config.sockets, device_config.socket_id, &dpdkx::config::sockets::socket::socket_id);
    if (socket_config == cend(sockets_config.sockets))
        socket_config = cbegin(sockets_config.sockets);
    

    adjust_to_socket(device_config, *socket_config);

    if (device_config.info.default_rxportconf.nb_queues > 1) {
        BOOST_LOG_SEV(log::get(), boost::log::trivial::warning) << device_config.info.default_rxportconf.nb_queues << " rx queues are configured but only single rx queue is supported at the moment";
        device_config.info.default_rxportconf.nb_queues = 1;
    }
    if (!options["multi-tx-queue"].as<bool>())
        device_config.info.default_txportconf.nb_queues = 1;

    constexpr auto svc_queues = 0;
    constexpr auto min_tx_queues = 1;
    auto detailed_stats = options["detailed-stats"].as<bool>();
    if (socket_config->cores.size() < device_config.info.default_rxportconf.nb_queues + min_tx_queues + svc_queues) {
        BOOST_LOG_SEV(log::get(), boost::log::trivial::error) << "not enough cores configured on socket - "<< socket_config->socket_id <<", please make sure that at least two cores";
        return;
    }

    if (auto n = socket_config->cores.size() - device_config.info.default_rxportconf.nb_queues + svc_queues;  n < device_config.info.default_txportconf.nb_queues) {
        BOOST_LOG_SEV(log::get(), boost::log::trivial::warning) << "not enough cores configured on socket - " << socket_config->socket_id << " to run " << device_config.info.default_txportconf.nb_queues << "tx queues, only " << n << " tx queues will be used ";
        device_config.info.default_txportconf.nb_queues = n;
    }

    auto cores_to_string = [](auto const& cores) {
        return std::accumulate(cbegin(cores), cend(cores), std::string{}, [](std::string str, auto const& core_id) {
            str += std::to_string(core_id);
            str += ' ';
            return str;
            });
        };    
    BOOST_LOG_SEV(log::get(), boost::log::trivial::debug) << device_config << "\twill run on socket " << socket_config->socket_id << " - " << socket_config->cores.size() << " cores : [" << cores_to_string(socket_config->cores) << ']';
    dpdkx::config::apply_workarounds(std::span{ &device_config, std::next(&device_config) });
    //device_config.info.rx_offload_capa &= ~(RTE_ETH_RX_OFFLOAD_VLAN_EXTEND | RTE_ETH_RX_OFFLOAD_TIMESTAMP | RTE_ETH_RX_OFFLOAD_TCP_LRO | RTE_ETH_RX_OFFLOAD_BUFFER_SPLIT);
    //device_config->info.rx_offload_capa = 0;
    BOOST_LOG_SEV(log::get(), boost::log::trivial::debug) << device_config.id << " port: " << device_config.port_id << " - requested offloads : " << dpdkx::io::rx_offloads{ device_config.effective_offload.rx };


    auto svc_mempool = dpdkx::make_scoped_mempool(dpdkx::svc_mempool_name(socket_config->socket_id).c_str(), 64 - 1, 24, 0, RTE_PKTMBUF_HEADROOM * 2 /*RTE_MBUF_DEFAULT_BUF_SIZE*/, socket_config->socket_id);
    auto mempool = dpdkx::shared_mempool{}; // all mempools used by device should outlive one!
    auto device = dpdkx::device{ device_config };
    if(device.clock_enabled())
        BOOST_LOG_SEV(log::get(), boost::log::trivial::debug) << "clock frequency " << device.clock_hz() << " hz" << (device.clock_bswap() ? " (byte swap)" :"");

    if(rte_eth_dev_set_ptypes(device.port_id(), RTE_PTYPE_UNKNOWN, NULL, 0) < 0)
        BOOST_LOG_SEV(log::get(), boost::log::trivial::warning) << "unable to disable Ptype parsing: " << dpdkx::last_error().message() << "...";

    // if (rte_eth_allmulticast_enable(device.port_id()) != 0)
    //     BOOST_LOG_SEV(log::get(), boost::log::trivial::error) << "port " << device.port_id() << " rte_eth_allmulticast_enable failed : " << dpdkx::last_error().message();

    if(rte_eth_promiscuous_enable(device.port_id())!=0)
        BOOST_LOG_SEV(log::get(), boost::log::trivial::warning) << "port " << device.port_id() <<" rte_eth_promiscuous_enable failed : " << dpdkx::last_error().message();
  
    if (device.start() != 0)
        throw std::system_error{ dpdkx::last_error() , "unable to start device : " + device.id() };

    // wait_for_link(device)
    auto link = device.link_status(/*timeout*/  std::chrono::seconds{ 9 });
    if (!link)
        throw std::system_error{ dpdkx::last_error() , "unable to get device : " + device.id() + " link status" };
    if (link->link_status == RTE_ETH_LINK_DOWN) 
        throw std::runtime_error{ device.id() + " [" + std::to_string(device.port_id()) + "] is down" };

    struct in_addr out_addr;
    out_addr.s_addr = device.ip4addr();
    BOOST_LOG_SEV(log::get(), boost::log::trivial::debug) << "port " << device.port_id() << " : " << inet_ntoa(out_addr) << " - link speed " << rte_eth_link_speed_to_str(link->link_speed);

    auto throughput_n_packets = options["throughput-test"].as<std::size_t>();
    auto latency_n_packets = options["latency-test"].as<std::size_t>();
    if (throughput_n_packets == 0 && latency_n_packets == 0) {
        latency_n_packets = default_latency_n_packets;
        throughput_n_packets = default_throughput_n_packets;
    }
    auto payload_size = options["packet-size"].as<std::size_t>();
    if (constexpr auto timestamps_size = sizeof(sockperf::x::timestamps); timestamps_size > payload_size)
        payload_size = timestamps_size;

    dpdkx::job_sentry job_sentry;
    
    sockaddr_in addr;
    addr.sin_family = AF_INET;
    addr.sin_port = rte_cpu_to_be_16(device.next_src_port() + 1);
    addr.sin_addr.s_addr = device.ip4addr();
    auto rx_channel = dpdkx::make_rx_channel<sockperf_channel>(device, addr, throughput_n_packets, detailed_stats);
    auto const& rx_jobs = device.rx_jobs();
    BOOST_LOG_SEV(log::get(), boost::log::trivial::debug) << "starting up " << rx_jobs.size() <<" rx queue(s)...";
    auto next_core = cbegin(socket_config->cores);
    for (auto job : rx_jobs) 
        rte_eal_remote_launch(&dpdkx::run_single_job, job, *next_core++);

    std::signal(SIGINT, [](auto) noexcept { 
        BOOST_LOG_SEV(log::get(), boost::log::trivial::warning) << "aborting...";
        dpdkx::stop_jobs(); 
     });

    auto tx_jobs = device.tx_jobs();
    rte_ether_addr mac_addr;
    if (auto destination_mac = options["destination-mac"]; !destination_mac.empty()) {
        if (auto addr = destination_mac.as<std::string>();  rte_ether_unformat_addr(addr.c_str(), &mac_addr) != 0) {
            BOOST_LOG_SEV(log::get(), boost::log::trivial::error) << "unable parse mac address " << addr;
            return;
        }
    } else {
        BOOST_LOG_SEV(log::get(), boost::log::trivial::info) << "obtaining mac address for " << inet_ntoa(dest_addr.sin_addr) << "...";
        if (auto error = dpdkx::ether_address(device, dest_addr.sin_addr.s_addr, mac_addr, tx_jobs)) {
            BOOST_LOG_SEV(log::get(), boost::log::trivial::error) << "unable to obtain address for " << inet_ntoa(dest_addr.sin_addr) << " : " << error.message();
            return;
        }
    }
    auto mac = std::array<char, RTE_ETHER_ADDR_FMT_SIZE>{};
    rte_ether_format_addr(mac.data(), mac.size(), &mac_addr);
    BOOST_LOG_SEV(log::get(), boost::log::trivial::info) << inet_ntoa(dest_addr.sin_addr) << " : " << mac.data();
    auto mempool_size_opt = options["tx-mempool-size"];
    assert(device.dev_info().default_txportconf.burst_size != 0);
    auto const mempool_size = mempool_size_opt.empty() ? static_cast<unsigned>(256 - 1) : mempool_size_opt.as<unsigned>() ;
    auto mempool_cache_size = device.dev_info().default_txportconf.burst_size * 16;
    if (auto half = (mempool_size + 1) / 2 ; mempool_cache_size > half)
        mempool_cache_size = half;
    if (mempool_cache_size > RTE_MEMPOOL_CACHE_MAX_SIZE / 4)
        mempool_cache_size = RTE_MEMPOOL_CACHE_MAX_SIZE / 4;
//    auto mempool_cache_size = 32;
    //[[maybe_unused]] auto svc_mempool = dpdkx::scoped_mempool{ rte_pktmbuf_pool_create(dpdkx::svc_mempool_name(socket->socket_id).c_str(), 64 - 1, 24, 0, RTE_PKTMBUF_HEADROOM * 2 /*RTE_MBUF_DEFAULT_BUF_SIZE*/, socket->socket_id), &rte_mempool_free };

    BOOST_LOG_SEV(log::get(), boost::log::trivial::debug) << "creating sockperf packets memory pool : " << mempool_size * tx_jobs.size() << " cache_size : " << mempool_cache_size;
    mempool = std::shared_ptr{ dpdkx::make_scoped_mempool("sockperf", mempool_size * tx_jobs.size(), mempool_cache_size, 0, RTE_PKTMBUF_HEADROOM + payload_size, device_config.socket_id) };
    //auto mempool = std::shared_ptr{ dpdkx::make_scoped_mempool("sockperf", 64 - 1, 24, 0, RTE_PKTMBUF_HEADROOM + payload_size /*RTE_MBUF_DEFAULT_BUF_SIZE*/, device_config.socket_id) };
    auto [pool_size, ol_flags] = configure_sockperf_packet_pool(device, mempool.get(), payload_size, std::make_pair(dest_addr.sin_addr.s_addr, mac_addr), static_cast<rte_be16_t>(dest_addr.sin_port), options["ttl"].as<std::uint8_t>());
    auto send_jobs = utils::workarounds::to<std::vector<tx_job>>(
        std::ranges::views::iota(dpdkx::queue_id_t{ 0 }, static_cast<dpdkx::queue_id_t>(tx_jobs.size()))
        | std::ranges::views::transform([&](auto const& queue_ix)
            { 
                return tx_job{ device, queue_ix, mempool, throughput_n_packets, ol_flags, payload_size };
            })
    )/*| std::ranges::to<std::vector<tx_job>>()*/;

    auto n_cores = (std::min)(socket_config->cores.size() - rx_jobs.size(), tx_jobs.size());
    if (n_cores < send_jobs.size()) {
        BOOST_LOG_SEV(log::get(), boost::log::trivial::error) << "not enough cores configured to run " << send_jobs.size() << " senders";
        return;
    }
    if (!options["no-warmup"].as<bool>()) {
        BOOST_LOG_SEV(log::get(), boost::log::trivial::info) << "warming up...";
        if (auto error = send_jobs.front().warmup(*rx_channel.get(), tx_jobs)) {
            BOOST_LOG_SEV(log::get(), boost::log::trivial::error) << error.message();
            return;
        }
    }

    if(latency_n_packets)
    {
        BOOST_LOG_SEV(log::get(), boost::log::trivial::info) << "performing latency test...";
        auto j = latency_test_job{ *rx_channel, 0/*queue_ix*/, mempool, latency_n_packets, ol_flags, payload_size };
        if (socket_config->cores.back() == rte_get_main_lcore())
            dpdkx::run_single_job(&j);
        else {
            rte_eal_remote_launch(&dpdkx::run_single_job, &j, *next_core);
            if (rte_eal_wait_lcore(*next_core) < 0)
                BOOST_LOG_SEV(log::get(), boost::log::trivial::error) << "rte_eal_wait_lcore(" << *next_core << ") failed : " << dpdkx::last_error().message() << "...";
        }
    }

    if (throughput_n_packets) {
        BOOST_LOG_SEV(log::get(), boost::log::trivial::info) << "performing throughput test...";
        auto const n_send_cores = send_jobs.size();
        auto send_cores = std::span{ next_core, socket_config->cores.back() == rte_get_main_lcore() ? n_send_cores - 1 : n_send_cores };
        auto next_send_job = begin(send_jobs);
        for (auto core : send_cores) 
            rte_eal_remote_launch(&dpdkx::run_single_job, &*next_send_job++, core);
        if (next_send_job != end(send_jobs))
            dpdkx::run_single_job(&*next_send_job);
        for (auto core : send_cores)
		    if (rte_eal_wait_lcore(core) < 0)
			    BOOST_LOG_SEV(log::get(), boost::log::trivial::error) << "rte_eal_wait_lcore("<<core<<") failed : " <<dpdkx::last_error().message() << "...";
    
        auto receive_core = cbegin(socket_config->cores);
        for (auto [effective_timeout, retries] = dpdkx::retry(options["wait-for"].as<std::chrono::milliseconds>()); rte_eal_get_lcore_state(*receive_core) != WAIT;) {
            if (retries-- == 0) {
                BOOST_LOG_SEV(log::get(), boost::log::trivial::warning) << "timed out, aborting...";
                dpdkx::stop_jobs();
                break;
            }
            rte_delay_ms(effective_timeout.count());
        } 
    } 
    dpdkx::stop_jobs();    

    std::for_each_n(cbegin(socket_config->cores), rx_jobs.size(), &rte_eal_wait_lcore);
    //if (rte_eal_wait_lcore(*receive_core) < 0)
    if (auto received_packets = rx_channel->packets_received()) {
        BOOST_LOG_SEV(log::get(), boost::log::trivial::info) << "processing collected data...";
        assert(received_packets == rx_channel->stats().requested_slots());
        rx_channel->stats().process();
    }
    
    if (struct rte_eth_stats stats; rte_eth_stats_get(device.port_id(), &stats) >= 0)
        BOOST_LOG_SEV(log::get(), boost::log::trivial::debug) << std::forward_as_tuple(device, stats);
    else
        BOOST_LOG_SEV(log::get(), boost::log::trivial::error) << " rte_eth_stats_get failed : " << dpdkx::last_error().message();

    //BOOST_LOG_SEV(log::get(), boost::log::trivial::info) << "...bye!";
}

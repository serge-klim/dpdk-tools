#define BOOST_TEST_MODULE dpdkx_tests
#include <boost/test/unit_test.hpp>
#include <boost/program_options.hpp>
#include <boost/process/v1/pipe.hpp>
#include <boost/process/v1/child.hpp>
#include <boost/process/v1/io.hpp>
#include <boost/algorithm/string.hpp>
#include <vector>
#include <string>
#include <iterator>
#include <filesystem>

std::string config_file() {
	std::string config;
	auto description = boost::program_options::options_description{ "options" };
	description.add_options()
		("config,c", boost::program_options::value<std::string>(&config)->default_value(std::string{}), "config file")
		;

	boost::program_options::positional_options_description positional;
	positional.add("config", -1);

	auto const& master_test_suite = boost::unit_test::framework::master_test_suite();
	boost::program_options::variables_map vm;
	boost::program_options::store(boost::program_options::command_line_parser(
		master_test_suite.argc,
		master_test_suite.argv).options(description).positional(positional).run(), vm);

	boost::program_options::notify(vm);
	return config;
}


BOOST_AUTO_TEST_SUITE(dpdkx_ftest_suite)

BOOST_AUTO_TEST_CASE(sockperf_cli_ftest)
{
	auto expected = std::vector<std::string>{
		std::string{ "[sockperfect_cli] pcap0 (net_pcap) port: 0 - 02:70:63:61:70:00[192.168.12.1]"},
		std::string{ "{"},
		std::string{ "	info : {"},
		std::string{ "		min_mtu : 46 (0x2e)"},
		std::string{ "		max_mtu : 65535 (0xffff)"},
		std::string{ "		min_rx_bufsize : 0"},
		std::string{ "		max_rx_bufsize : 4294967295 (0xffffffff)"},
		std::string{ "		max_rx_pktlen : 4294967295 (0xffffffff)"},
		std::string{ "		max_lro_pkt_size : 0"},
		std::string{ "		max_rx_queues : 1"},
		std::string{ "		max_tx_queues : 1"},
		std::string{ "		max_mac_addrs : 1"},
		std::string{ "		max_hash_mac_addrs : 0"},
		std::string{ "		max_vfs : 0"},
		std::string{ "		max_vmdq_pools : 0"},
		std::string{ "		rx_offload_capa : 0"},
		std::string{ "		tx_offload_capa : 0"},
		std::string{ "		rx_queue_offload_capa : 0"},
		std::string{ "		tx_queue_offload_capa : 0"},
		std::string{ "		reta_size : 0"},
		std::string{ "		rss_algo_capa : 1"},
		std::string{ "		flow_type_rss_offloads : 0"},
		std::string{ "		default_rxconf : {"},
		std::string{ "			rx_thresh : {"},
		std::string{ "				pthresh : 0"},
		std::string{ "				hthresh : 0"},
		std::string{ "				wthresh : 0"},
		std::string{ "			}"},
		std::string{ "			rx_free_thresh : 0"},
		std::string{ "			rx_drop_en : 0"},
		std::string{ "			rx_deferred_start : 0"},
		std::string{ "			rx_nseg : 0"},
		std::string{ "			share_group : 0"},
		std::string{ "			share_qid : 0"},
		std::string{ "			offloads : 0"},
		std::string{ "			rx_nmempool : 0"},
		std::string{ "		}"},
		std::string{ "		default_txconf : {"},
		std::string{ "			tx_thresh : {"},
		std::string{ "				pthresh : 0"},
		std::string{ "				hthresh : 0"},
		std::string{ "				wthresh : 0"},
		std::string{ "			}"},
		std::string{ "			tx_rs_thresh : 0"},
		std::string{ "			tx_free_thresh : 0"},
		std::string{ "			tx_deferred_start : 0"},
		std::string{ "			offloads : 0"},
		std::string{ "		}"},
		std::string{ "		vmdq_queue_base : 0"},
		std::string{ "		vmdq_queue_num : 0"},
		std::string{ "		vmdq_pool_base : 0"},
		std::string{ "		rx_desc_lim : {"},
		std::string{ "			nb_max : 65535 (0xffff)"},
		std::string{ "			nb_min : 0"},
		std::string{ "			nb_align : 1"},
		std::string{ "			nb_seg_max : 65535 (0xffff)"},
		std::string{ "			nb_mtu_seg_max : 65535 (0xffff)"},
		std::string{ "		}"},
		std::string{ "		tx_desc_lim : {"},
		std::string{ "			nb_max : 65535 (0xffff)"},
		std::string{ "			nb_min : 0"},
		std::string{ "			nb_align : 1"},
		std::string{ "			nb_seg_max : 65535 (0xffff)"},
		std::string{ "			nb_mtu_seg_max : 65535 (0xffff)"},
		std::string{ "		}"},
		std::string{ "		speed_capa : 0"},
		std::string{ "		nb_rx_queues : 1"},
		std::string{ "		nb_tx_queues : 1"},
		std::string{ "		default_rxportconf : {"},
		std::string{ "			burst_size : 0"},
		std::string{ "			ring_size : 0"},
		std::string{ "			nb_queues : 1"},
		std::string{ "		}"},
		std::string{ "		default_txportconf : {"},
		std::string{ "			burst_size : 0"},
		std::string{ "			ring_size : 0"},
		std::string{ "			nb_queues : 1"},
		std::string{ "		}"},
		std::string{ "		dev_capa : 0"},
		std::string{ "	}"},
		std::string{ "	effective_offload : {"},
		std::string{ "		rx : 0"},
		std::string{ "		tx : 0"},
		std::string{ "	}"},
		std::string{ "	svc_tx_n : 1"},
		std::string{ "	next_src_port : 49152 (0xc000)"},
		std::string{ "	rx_reconfig_hint : 1024 (0x400)"},
		std::string{ "	clock_hz : 1410065408 (0x540be400)"},
		std::string{ "	rx_meta_features : 0"},
		std::string{ "	features : 0000000000000000000000000000000000000000000000000000000000000110"},
		std::string{ "}	will run on socket 0 - 6 cores : [7 1 2 4 6 0 ]"},
		std::string{ "[sockperfect_cli] pcap0 port: 0 - requested offloads : 0"},
		std::string{ "device 0 - net_pcap doesn't support clock, disabling..."},
		std::string{ "starting up port 0 ..."},
		std::string{ "[sockperfect_cli] port 0 : 192.168.12.1 - link speed 10 Gbps"},
		std::string{ "[sockperfect_cli] starting up 1 rx queue(s)..."},
		std::string{ "running single job on core : 7"},
		std::string{ "[sockperfect_cli] obtaining mac address for 192.168.12.2..."},
		std::string{ "[sockperfect_cli] 192.168.12.2 : D8:43:AE:11:8F:6F"},
		std::string{ "[sockperfect_cli] performing latency test ..."},
		std::string{ "[sockperfect_cli] performing throughput test..."},
		std::string{ "[sockperfect_cli] tx queue: 0:packet size: 74 bytes / payload size: 32 bytes "},
		std::string{ "[stats] tx queue: 0:211077 packets are missing out of 221077 packets"},
		std::string{ "packet to packet interval (hardware timestamp):"},
		std::string{ "  4 : [ 1 ns-2 ns) : 11"},
		std::string{ "  5 : [ 2 ns-3 ns) : 5"},
		std::string{ "  6 : [ 3 ns-4 ns) : 142"},
		std::string{ "  7 : [ 4 ns-5 ns) : 1646"},
		std::string{ "  8 : [ 5 ns-6 ns) : 2107"},
		std::string{ "  9 : [ 6 ns-7 ns) : 4625"},
		std::string{ " 10 : [ 7 ns-8 ns) : 1237"},
		std::string{ " 11 : [ 8 ns-9 ns) : 138"},
		std::string{ " 12 : [ 9 ns-10 ns) : 23"},
		std::string{ " 13 : [ 10 ns-11 ns) : 28"},
		std::string{ " 14 : [ 11 ns-12 ns) : 11"},
		std::string{ " 15 : [ 12 ns-13 ns) : 1"},
		std::string{ " 16 : [ 13 ns-15 ns) : 5"},
		std::string{ " 17 : [ 15 ns-20 ns) : 5"},
		std::string{ " 20 : [ 30 ns-35 ns) : 2"},
		std::string{ " 22 : [ 40 ns-45 ns) : 2"},
		std::string{ " 32 : [ 90 ns-95 ns) : 1"},
		std::string{ " 44 : [ 1250 ns-1500 ns) : 1"},
		std::string{ " 45 : [ 1500 ns-3228 ns) : 8"},
		std::string{ " 46 : [ 3228 ns-4092 ns) : 1"},
		std::string{ "    ---------------"},
		std::string{ "	9999.000000"},
		std::string{ "packet round trip (rdtsc):"},
		std::string{ "  0 : [ -9223372036854775808 ns--6280589492311827456 ns) : 1579"},
		std::string{ "  1 : [ -6280589492311827456 ns--100 us) : 3411"},
		std::string{ " 47 : [ 1500 us-17826109.575559 min) : 614"},
		std::string{ " 48 : [ 17826109.575559 min-73065573.755025 min) : 1756"},
		std::string{ " 49 : [ 73065573.755025 min-122300238.431331 min) : 1592"},
		std::string{ " 50 : [ 122300238.431331 min-152319544.697523 min) : 998"},
		std::string{ " 51 : [ 152319544.697523 min--9223372036854775808 ns) : 50"},
		std::string{ "    ---------------"},
		std::string{ "	10000.000000"},
		std::string{ "[stats] 10000 packets has been received in"},
		std::string{ "	Total number of successfully received packets: 37728"},
		std::string{ "	Total number of successfully transmitted packets: 10018"},
		std::string{ "	Total number of successfully received bytes: 2791898"},
		std::string{ "	Total number of successfully transmitted bytes: 741300"},
		std::string{ "	missed : 0 from : 37728 - 0%"},
		std::string{ "	Total number of erroneous received packets: 0"},
		std::string{ "	Total number of failed transmitted packets: 0"},
		std::string{ "	Total number of Rx mbuf allocation failures: 0"},
		std::string{ "		0 Total number of queue Rx packets : 37728"},
		std::string{ "		0 Total number of successfully received queue bytes : 2791898"},
		std::string{ "		0 Total number of queue packets received that are dropped : 0"},
		std::string{ "		0 Total number of queue Tx packets : 10018"},
		std::string{ "		0 Total number of successfully transmitted queue bytes : 741300"},
};
	
	auto path = std::filesystem::current_path();
	path /= "sockperf_cli.exe";
	BOOST_REQUIRE_MESSAGE(exists(path), std::string{"couldn't find path : "} + path.string());
    boost::process::v1::ipstream pipe;
	auto cmd = path.string();
	cmd += " --detailed-stats";
	if (auto config = config_file(); !config.empty()) {
		cmd += " -c ";
		cmd += config;
	}
//    auto cmd = std::string{ "C:\\Users\\serge\\Documents\\projects\\dpdk-tools\\out\\build\\clang-winx64-debug\\sockperf_cli\\sockperf_cli.exe -c C:\\Users\\serge\\Documents\\projects\\dpdk-tools\\sockperf-ping.config -l 3 -t -v --detailed-stats"};
    boost::process::v1::child c(
        cmd,
        boost::process::v1::std_out > pipe,
        boost::process::v1::std_err > pipe);

    std::string in{ std::istreambuf_iterator<char>{pipe}, std::istreambuf_iterator<char>{} };
    c.wait();
	auto pos = 0;
	for (auto const& str : expected) {
		auto current = in.find(str, pos);
		BOOST_CHECK_MESSAGE(current != std::string::npos, "can't find : " + str);
		if (current != std::string::npos)
			pos = current + str.length();			
	}
	//auto in_strings = std::vector<std::string>{};
	//boost::algorithm::erase_all(in, "\r");
	//boost::algorithm::split(in_strings,in, boost::is_any_of("\n"));
 //   BOOST_CHECK_EQUAL_COLLECTIONS(cbegin(in_strings), cend(in_strings), cbegin(expected_output), cend(expected_output));
}


BOOST_AUTO_TEST_SUITE_END()

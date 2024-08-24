#include "utils/program_options/validators/chrono.hpp"
#include "utils/program_options/validators/net.hpp"
#include "utils/program_options/log.hpp"
//#include <boost/program_options.hpp>
#include "loggers.hpp"
#include <boost/format.hpp>
#include <list>
#include <iostream>
#include <fstream>
#include <filesystem>
#include <string>


boost::program_options::options_description common_options()
{
   auto description = boost::program_options::options_description{ "configuration" };
   // clang-format off
   description.add_options()
       ("destination,d", boost::program_options::value<net::endpoint>()->required(), "\tdestination endpoint")
       ("destination-mac,m", boost::program_options::value<std::string>(), "\tdestination mac address")
       ("ttl", boost::program_options::value<std::uint8_t>()->default_value(64), "\tttl")
       ("packet-size,s", boost::program_options::value<std::size_t>()->default_value(32), "\tuse <size> as number of data bytes to be sent")
       ("latency-test,l", boost::program_options::value<std::size_t>()->default_value(0)->implicit_value(default_latency_n_packets), "\tlatency (ping like) test [number of packets to send]")
       ("throughput-test,t", boost::program_options::value<std::size_t>()->default_value(0)->implicit_value(default_throughput_n_packets), "\tthroughput test (burst send)[number of packets requests to send]")
       ("no-warmup", boost::program_options::value<bool>()->default_value(false)->implicit_value(true), "\tskip warmup")
       ("burst-size,b", boost::program_options::value<std::uint16_t>(), "\trx burst size")
       //("multi-tx-queue", boost::program_options::value<bool>()->implicit_value(true)->default_value(false), "\ttx multi queues")
       ("txq", boost::program_options::value<std::uint16_t>()->default_value(1), "\tnumber of tx queues")
       ("tx-mempool-size", boost::program_options::value<unsigned>()->default_value(1024 * 4 - 1), "\tnumber of element in rx memory pool(per tx queues)")
       ("wait-for,w", boost::program_options::value<std::chrono::milliseconds>()->default_value(std::chrono::seconds{ 5 }, "5 sec"), "\twait for response")
       ("detailed-stats", boost::program_options::value<bool>()->default_value(false)->implicit_value(true), "\tdetailed stats")
       ("buckets,b", boost::program_options::value<std::list<std::chrono::nanoseconds>>()/*->implicit_value(std::list<std::chrono::nanoseconds>{},"")*/, "\thistogram buckets for packet to packet timing," 
                                                                                                                                                          " displayed when detailed-stats option is selected")
       ("verbose,v", boost::program_options::value<bool>()->default_value(false)->implicit_value(true), "\tverbose")
       ;
   // clang-format on
   return description;
}

boost::program_options::options_description eal_options()
{
    auto description = boost::program_options::options_description{ "eal" };
    // clang-format off
    description.add_options()
        ("eal.*", boost::program_options::value<std::string>()->implicit_value(std::string{}), "https://doc.dpdk.org/guides/linux_gsg/linux_eal_parameters.html")
        ("eal.vdev,vdev", boost::program_options::value<std::vector<std::string>>()->multitoken(), "add a virtual device(s)")
        ;
    // clang-format on
    return description;
}

std::ostream& help(std::ostream& out, boost::program_options::options_description& description)
{
   // clang-format off
    out << description
        << "\n\n more details: https://github.com/serge-klim/dpdk-tools/edit/main/README.md \n";
   // clang-format on
   return out;
}

bool parse_config(boost::program_options::options_description const& description,boost::program_options::variables_map& vm)
{
   auto res = false;
   auto const& config_opt = vm["config"];
   auto config_filename = config_opt.empty() ? std::make_pair(false, std::string{"config.ini"}) : std::make_pair(true, config_opt.as<std::string>());
   if (auto ifs = std::ifstream{config_filename.second}) {

      auto parsed = parse_config_file(ifs, description, true);
      store(parsed, vm);

      //auto const& additional = collect_unrecognized(parsed.options, boost::program_options::include_positional);
      //init_log_from_unrecognized_program_options(additional);
      if (!init_log_from_unrecognized_program_options(parsed, vm))
          configure_default_logger(vm);

      //notify(vm); // check config file options sanity
      res = true;
   } else if (config_filename.first)
      throw std::runtime_error{str(boost::format("can't open configuration file \"%1%\"") % config_filename.second)};
   return res;
}

//#pragma message ("TODO: move to utility!!!!!!!!!!!!")
//void add_unrecognized_program_options(boost::program_options::basic_parsed_options<char> const& parsed_options, boost::program_options::variables_map& options_map) {
//    for (auto const& option : parsed_options.options) {
//        if (option.unregistered /* ||
//             (mode == include_positional && options[i].position_key != -1)
//             */
//            ) {
//            if (option.original_tokens.size() == 2)
//                options_map.emplace(option.original_tokens[0], boost::program_options::variable_value{ boost::any{option.original_tokens[1]}, false });
//        }
//    }
//}


int main(int argc, char* argv[])
{
   auto description = boost::program_options::options_description{ "general" };
   try {
      // clang-format off
        description.add_options()
            ("help,h", "\tprint usage message")
            ("config,c", boost::program_options::value<std::string>(), "\tconfiguration file")
            ;
      // clang-format on

     auto config_file_options = common_options();
     auto eal_opts = eal_options();
     config_file_options.add(eal_opts);
     description.add(config_file_options);

     boost::program_options::positional_options_description positional;
     positional.add("destination", -1);
     
      
     auto parsed = boost::program_options::command_line_parser(
         argc, argv)
         .options(description).positional(positional).run();
     boost::program_options::variables_map vm;
     store(parsed, vm);

     if (vm.count("help") != 0) {
        help(std::cout, description);
        return 0;
     }
     if (!parse_config(config_file_options, vm) && !init_log_from_unrecognized_program_options(parsed, vm))
         /*add_unrecognized_program_options(parsed, vm);*/
         configure_default_logger(vm);

     notify(vm); // check cmd-line option sanity
     void run(boost::program_options::variables_map const& options);
     run(vm);

   } catch (boost::program_options::error& e) {
      std::cerr << "error : " << e.what() << "\n\n";
      help(std::cerr, description);
   } catch (std::exception& e) {
      std::cerr << "error : " << e.what() << std::endl;
   } catch (...) {
      std::cerr << "miserably failed:(" << std::endl;
   }

   return 0;
}

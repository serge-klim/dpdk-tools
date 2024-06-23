#include "io.hpp"
#include "../io.hpp"
#include "program_options.hpp"
#include "netinet_in.hpp"
#include "loggers.hpp"
#include "error.hpp"
#include "rte_ethdev.h"
#include "parsers/parser.hpp"
#include <boost/spirit/home/x3.hpp>
#include <boost/fusion/adapted/struct/detail/extension.hpp>
#include <boost/fusion/include/is_sequence.hpp>
#include <boost/fusion/include/size.hpp>
#include <boost/fusion/include/value_at.hpp>
#include <boost/fusion/include/at_c.hpp>
#include <boost/mp11/algorithm.hpp>
//#include <boost/pfr.hpp>
//#include <boost/pfr/core_name.hpp>
#include <string_view>
#include <ranges>
#include <type_traits>

dpdkx::v0::config::device_matcher::device_matcher(std::string id) 
    : id_{ std::move(id) } 
{
    rte_ether_addr mac_addr;
    if (rte_ether_unformat_addr(id_.c_str(), &mac_addr) == 0)
        mac_.assign(mac_addr.addr_bytes, mac_addr.addr_bytes + sizeof(mac_addr.addr_bytes) / sizeof(mac_addr.addr_bytes[0]));

    in_addr addr;
    if (inet_pton(AF_INET, id_.c_str(), &addr) == 1)
        addr_ = addr;
}

bool dpdkx::v0::config::device_matcher::operator ()(utils::net::nic_info const& info) const {
    return info.name == id_
        || (!mac_.empty() && std::ranges::equal(info.mac, mac_))
        || (!info.ipv4.empty() && addr_ && std::ranges::any_of(info.ipv4, [addr = *addr_](auto const& ipv4) {
                return addr.s_addr == ipv4.s_addr;
            }));            
}

bool dpdkx::v0::config::device_matcher::operator ()(rte_ether_addr const& mac_addr) const {
    return !mac_.empty() && std::equal(cbegin(mac_), cend(mac_), mac_addr.addr_bytes, mac_addr.addr_bytes + sizeof(mac_addr.addr_bytes) / sizeof(mac_addr.addr_bytes[0]));
}

bool dpdkx::v0::config::device_matcher::operator ()(device const& dev) const {
    return dev.id == id_
        //|| (!mac_.empty() && std::ranges::equal(info.mac, mac_))
        || (addr_ && dev.ip4_addr == addr_->s_addr);

}


std::vector<utils::net::nic_info> dpdkx::v0::config::get_nic_info(boost::program_options::variables_map const& options) {
    auto nics_info = utils::net::get_nic_info();
    auto options_end = cend(options);
    for (auto i = cbegin(options); i != options_end; ++i) {
        static auto const nic = std::string{ "nic." };
        if (i->first.starts_with(nic)) {
            auto nic_id_begin = next(cbegin(i->first), nic.length());
            auto end = cend(i->first);
            auto nic_id_end = std::find(nic_id_begin, end, '.');
            if (std::string_view{ nic_id_end, end } == ".ip") {
                auto const mac = std::string{ nic_id_begin, nic_id_end };
                rte_ether_addr mac_addr;
                if (rte_ether_unformat_addr(mac.c_str(), &mac_addr) != 0) {
                    logging::logger<config_log>().warn("can't parse mac address : {}", mac);
                    continue;
                }
                auto ipv4_addresses = std::vector<in_addr>(1);
                if (auto address = i->second.as<std::string>(); inet_pton(AF_INET, address.c_str(), &ipv4_addresses[0]) == 0) {
                    logging::logger<config_log>().warn("can't parse ip4 address : {}", address);
                    continue;
                }

                auto nics_end = std::end(nics_info);
                auto info = std::find_if(begin(nics_info), nics_end, [&mac_addr](auto const& info) {
                    return std::equal(cbegin(info.mac), cend(info.mac), mac_addr.addr_bytes, mac_addr.addr_bytes + sizeof(mac_addr.addr_bytes) / sizeof(mac_addr.addr_bytes[0]));
                    });
                if (info == nics_end) {
                    nics_info.push_back(utils::net::nic_info{});
                    info = std::end(nics_info);
                    --info;
                    info->mac.reserve(sizeof(mac_addr.addr_bytes) / sizeof(mac_addr.addr_bytes[0]));
                    std::copy(mac_addr.addr_bytes, mac_addr.addr_bytes + sizeof(mac_addr.addr_bytes) / sizeof(mac_addr.addr_bytes[0]), std::back_inserter(info->mac));
                }
                info->ipv4 = std::move(ipv4_addresses);
                if (auto ipv6 = options[std::string{ cbegin(i->first), nic_id_end } + ".ip6"]; !ipv6.empty()) {
                    auto ipv6_addresses = std::vector<in6_addr>(1);
                    if (auto address = ipv6.as<std::string>(); inet_pton(AF_INET6, address.c_str(), &ipv6_addresses[0]) != 0) {
                        info->ipv6 = std::move(ipv6_addresses);
                    }
                    else {
                        logging::logger<config_log>().warn("can't parse ip6 address : {}", address);
                    }
                }
                if (auto name = options[std::string{ cbegin(i->first), nic_id_end } + ".name"]; !name.empty())
                    info->name = name.as<std::string>();
            }
        }
    }
    return nics_info;
}
namespace {
std::string device_key(dpdkx::v0::config::device const& dev) {
    auto mac = std::array<char, RTE_ETHER_ADDR_FMT_SIZE>{};
    rte_ether_format_addr(mac.data(), mac.size(), &dev.mac_addr);
    auto key = std::string{ "nic." };
    key += mac.data();
    return key;
}

} // namespace

/////////////////////////////////////////////////////////////////




template<typename T, std::size_t Bits>
bool parse_value(std::string const& text, std::bitset<Bits>& val) {
    val = std::bitset<Bits>{ text };
    return true;
}

template<typename T, typename Val>
bool parse_value(std::string const& text, Val& val) {
    auto begin = cbegin(text);
    auto end = cend(text);
    return boost::spirit::x3::parse(begin, end, boost::spirit::x3::expect[parser::type<T>{}()], val) && end == begin;
}

template<typename T, typename Val>
void load_value_as(boost::program_options::variables_map const& options, std::string const& path, Val& val) {
    if (auto value = options[path]; !value.empty()) {
        auto const& text = value.as<std::string>();
        try{
            if (!parse_value<T>(text, val))
                throw std::runtime_error{ "unable to parse \"" + path + "\" : " + text};
        }
        catch (boost::spirit::x3::expectation_failure<std::string::const_iterator> const& e) {
            auto pos = std::distance(std::cbegin(text), e.where());
            std::stringstream message;
            message << "unable to parse \"" << path << "\", invalid " << e.which() << " value : "
                << "\n\t" << text
                << "\n\t" << std::setw(pos) << std::setfill('-') << ""
                << "^~~~~\n";
            throw std::runtime_error{ message.str() };
        }
    }
}

template<typename T, typename Val>
requires(boost::describe::has_describe_members<T>::value)
void load_value_as(boost::program_options::variables_map const& options, std::string const& path, Val& val) {
    boost::mp11::mp_for_each<boost::describe::describe_members<T, boost::describe::mod_public>>([&options, &path, &val](auto member) {
        auto key = path;
        key += '.';
        key += member.name;
        //static_cast<io::type_traits<T, std::decay_t<decltype(val.*member.pointer)>, decltype(member)::pointer>::type>(val.*member.pointer);
        using value_type = std::decay_t<decltype(val.*member.pointer)>;
        load_value_as<typename io::io_type<decltype(member)::pointer>::type, value_type>(options, key, val.*member.pointer);
    });
}

template<typename T>
T& load_value(boost::program_options::variables_map const& options, std::string const& path, T& val) {
    load_value_as<T,T>(options, path, val);
    return val;
}

//boost::program_options::variable_value 

/////////////////////////////////////////////////////////////////

std::vector<dpdkx::v0::config::device> dpdkx::v0::config::devices_configuration(std::vector<utils::net::nic_info> const& nics_info, boost::program_options::variables_map const& options /*= {}*/) {

    auto res = std::vector<dpdkx::v0::config::device>{};
    //lookup in and out ports by provided id
    //auto in_port = std::make_tuple(std::uint16_t{ RTE_MAX_ETHPORTS }, rte_ether_addr{}, static_cast<utils::net::nic_info const*>(nullptr));
    //auto out_port = std::make_tuple(std::uint16_t{ RTE_MAX_ETHPORTS }, rte_ether_addr{}, static_cast<utils::net::nic_info const*>(nullptr));
    auto port_id = std::uint16_t{};
    RTE_ETH_FOREACH_DEV(port_id) {
        auto dev = config::device{};
        dev.port_id = port_id;
        dev.socket_id = static_cast<unsigned int>(rte_eth_dev_socket_id(port_id));
        if (rte_eth_macaddr_get(port_id, &dev.mac_addr) != 0)
            throw std::system_error{ dpdkx::last_error() , "rte_eth_dev_info_get failed" };

        auto end = cend(nics_info);
        auto info = std::find_if(cbegin(nics_info), end, [&dev](auto const& info) {
            return std::equal(cbegin(info.mac), cend(info.mac), dev.mac_addr.addr_bytes, dev.mac_addr.addr_bytes + sizeof(dev.mac_addr.addr_bytes) / sizeof(dev.mac_addr.addr_bytes[0]));
        });
        if (info != end) {
            dev.id = info->name;
            if(!info->ipv4.empty())
                dev.ip4_addr = info->ipv4.front().s_addr;
            if (!info->ipv6.empty()) {
                static_assert( sizeof(info->ipv6.front().s6_addr) == sizeof(dev.ip6_addr)/sizeof(dev.ip6_addr[0]), "ipv4 part of device info has to be fixed");
                auto ipv6 = info->ipv6.front().s6_addr;
                std::transform(ipv6, ipv6 + sizeof(dev.ip6_addr) / sizeof(dev.ip6_addr[0]), dev.ip6_addr, [](auto b) {
                    return static_cast<std::uint8_t>(b);
                });
            }
        }
        if (rte_eth_dev_info_get(port_id, &dev.info) != 0)
            throw std::system_error{ dpdkx::last_error() , "rte_eth_dev_info_get failed" };
        if (dev.id.empty())
            dev.id = std::to_string(port_id);

        //boost::mp11::mp_for_each<boost::mp11::mp_iota_c<boost::pfr::tuple_size_v<decltype(dev.info)>>>([&dev, key = device_key(dev)](auto I) {
        //    auto k = key;
        //    k += ".info.";
        //    k += boost::pfr::get_name<I, decltype(dev.info)>();
        //});
        //test(options["dev_info"]);
        auto const drv_key = std::string{ dev.info.driver_name };
        load_value(options, drv_key, dev);
        if (auto value = options[drv_key + ".features"]; !value.empty())
            dev.features = decltype(dev.features){value.as<std::string>()};

        auto const dev_key = device_key(dev);
        load_value(options, dev_key, dev);
        //load_value(options, dev_key + ".dev_info",dev.info);
        if (auto value = options[dev_key + ".features"]; !value.empty())
            dev.features = decltype(dev.features){value.as<std::string>()};
        //load_value(options, dev_key + ".next_src_port", dev.next_src_port);
        if (dev.info.default_rxportconf.nb_queues == 0)
            dev.info.default_rxportconf.nb_queues = dev.info.max_rx_queues;
        if (dev.info.default_txportconf.nb_queues == 0)
            dev.info.default_txportconf.nb_queues = dev.info.max_tx_queues;
        //load_value(options, dev_key + ".svc_tx_n", dev.svc_tx_n);
        if (dev.svc_tx_n > dev.info.default_txportconf.nb_queues)
            dev.svc_tx_n = dev.info.default_txportconf.nb_queues;

        dev.effective_offload.rx &= dev.info.rx_offload_capa;
        dev.effective_offload.tx &= dev.info.tx_offload_capa;

        if (auto error = rte_eth_rx_metadata_negotiate(port_id, &dev.rx_meta_features); error < 0) {
            logging::logger<config_log>().warn("unable negotiate rx meta features on port {} : {}", port_id, dpdkx::make_error_code(-error).message());
            dev.rx_meta_features = 0;
        }
        res.push_back(std::move(dev));
    }
    return res;
}

std::vector<std::string> dpdkx::v0::config::eal_option(std::string app_name, boost::program_options::variables_map const& options) {
    //"02:70:63:61:70:00"	pcap/x0
    //"02:70:63:61:70:01"	pcap/x1
    auto eal_strings = std::vector<std::string>{ std::move(app_name)};
    static const auto eal_prefix = std::string{ "eal." };
    for (auto const& [key, value] : options | std::views::filter([](auto const& opt) { return opt.first.starts_with(eal_prefix); })) {
        if (auto len = key.length() - eal_prefix.length()) {
            auto name = len == 1 ? std::string{ '-' } : std::string{ "--" };
            name += key.substr(eal_prefix.length());
            eal_strings.push_back(name);
            if (!value.empty()) {
                try {
                    if (auto val = value.as<std::string>(); !val.empty())
                        eal_strings.push_back(std::move(val));
                    continue;
                }
                catch (boost::bad_any_cast const&) {}
                auto vals = value.as<std::vector<std::string>>();
                auto i = cbegin(vals);
                eal_strings.push_back(*i);
                auto end = cend(vals);
                while (++i != end) {
                    eal_strings.push_back(name);
                    eal_strings.push_back(*i);
                }
            }
        }
    }
    return eal_strings;
}

void dpdkx::v0::config::apply_workarounds(std::span<dpdkx::v0::config::device> devices, boost::program_options::variables_map const& /*options*/ /*= {}*/) {
    
    auto net_igcs = devices | std::views::filter([](device const& dev) {
            static constexpr auto net_igc = std::string_view{ "net_igc" };
            return net_igc == dev.info.driver_name;
        });
    std::ranges::for_each(net_igcs, [](device& dev) {
        dev.info.rx_offload_capa &= ~(RTE_ETH_RX_OFFLOAD_VLAN_EXTEND | RTE_ETH_RX_OFFLOAD_TIMESTAMP | RTE_ETH_RX_OFFLOAD_TCP_LRO | RTE_ETH_RX_OFFLOAD_BUFFER_SPLIT);
    });
}

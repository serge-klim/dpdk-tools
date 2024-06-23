#include "../io.hpp"
#include "io.hpp"
#include "netinet_in.hpp"
#include "rte_ethdev.h"
#include <boost/mp11/algorithm.hpp>
#include <array>
#include <iostream>
#include <sstream>

namespace {

static char const* prefixes = "\t\t\t\t\t\t\t";
thread_local auto prefix = std::char_traits<char>::length(prefixes);

}

template<typename T>
auto operator << (std::ostream& out, T const& val) -> std::enable_if_t<boost::describe::has_describe_members<T>::value, std::ostream&> {
    if (prefix !=0 ) {
        out << '{';
        --prefix;
        boost::mp11::mp_for_each<boost::describe::describe_members<T, boost::describe::mod_public>>([&out, &val](auto member) {
            //detail::member_type(member.pointer);
            out << '\n' << prefixes + prefix << member.name << " : ";
            using value_type = typename io::io_type<decltype(member)::pointer>::type/*std::decay_t<decltype(val.*member.pointer)>*/;
            if constexpr (std::is_integral_v<value_type> && std::is_unsigned_v<value_type> && sizeof(value_type) > sizeof(char)) {
                out << std::dec << static_cast<unsigned int>(val.*member.pointer);
                if(val.*member.pointer >= 0xa)
                    out << " (0x" << std::hex << static_cast<unsigned int>(val.*member.pointer) << ')';
            }
            else
                out << static_cast<typename io::io_type<decltype(member)::pointer>::type>(val.*member.pointer);
            });
        ++prefix;
        out << '\n' << prefixes + prefix << '}';
    } else
        out << '\n' << prefixes + prefix << "{...}\n";
    return out;
}

std::ostream& dpdkx::v0::config::operator << (std::ostream& out, dpdkx::v0::config::device const& dev) {

	auto mac = std::array<char, RTE_ETHER_ADDR_FMT_SIZE>{};
	rte_ether_format_addr(mac.data(), mac.size(), &dev.mac_addr);
    auto in_ip4 = dev.ip4_addr ? std::string{ inet_ntoa(*reinterpret_cast<in_addr const*>(&dev.ip4_addr)) } : std::string{ "<not set>" };
    out << dev.id << " (" << dev.info.driver_name << ") port: " << dev.port_id << " - " << mac.data() << "[" << in_ip4 << "]\n"// << "] will run on socket " << in_socket->socket_id << " - " << in_socket->cores.size() << " cores : [" << cores_to_string(in_socket->cores) << ']'
        // << "\nrx : " << dev.info.max_rx_queues << " - queue(s), supported offloads : " << to_string(dpdkx::io::rx_offloads{dev.info.rx_offload_capa })
        // << "\ntx : " << dev.info.max_tx_queues << " - queue(s), supported offloads : " << to_string(dpdkx::io::tx_offloads{dev.info.tx_offload_capa })
        // << dev.info
		;
	::operator <<<dpdkx::v0::config::device>(out, dev);
    return out;
}

std::string dpdkx::v0::config::to_string(device const& dev) {
    std::stringstream out;
    out << dev;
    return out.str();
}

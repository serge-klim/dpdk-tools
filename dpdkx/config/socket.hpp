#pragma once
#include "log.hpp"
#include "rte_ether.h"
#include "rte_ethdev.h"
#include <span>
#include <vector>
#include <cstdint>
#include <cstddef>


namespace dpdkx { inline namespace v0 { namespace config {

struct sockets {
    struct socket {
        unsigned int socket_id = static_cast<unsigned int>(SOCKET_ID_ANY);
        std::span<std::uint32_t const> cores;
    };
    std::vector<socket> sockets;
    std::vector<std::uint32_t> cores;
};

sockets socket_configuration();
void adjust_to_socket(struct device& dev, sockets::socket const& socket);

}}} // namespace dpdkx::v0::config


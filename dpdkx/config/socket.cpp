#include "socket.hpp"
#include "device.hpp"
#include <rte_ethdev.h>
#include <algorithm>
#include <functional>
#include <iterator>
#include <cassert>


dpdkx::v0::config::sockets dpdkx::v0::config::socket_configuration() {
    struct core_config {
        unsigned int socket_id = static_cast<unsigned int>(SOCKET_ID_ANY);
        std::size_t cores_n = 0;
    };
    std::vector<core_config> sockets;
    auto res = dpdkx::v0::config::sockets{};

    auto lcore_id = std::uint32_t{};
    RTE_LCORE_FOREACH/*RTE_LCORE_FOREACH_WORKER*/(lcore_id) {
        auto socket_id = rte_lcore_to_socket_id(lcore_id);

        auto core = begin(res.cores);        
        auto sockets_end = end(sockets);
        for (auto socket = begin(sockets);;++socket) {
            if (socket == sockets_end) {
                sockets.emplace_back(socket_id, 1);
                res.cores.push_back(lcore_id);
                break;
            }
            std::advance(core, socket->cores_n);
            if (socket->socket_id == socket_id) {
                res.cores.insert(core,lcore_id);
                ++socket->cores_n;
                break;
            }
        }
    }

    auto main_core = rte_get_main_lcore();
    auto main_core_socket_id = rte_lcore_to_socket_id(main_core);

    auto offset = std::size_t{ 0 };
    auto const n = sockets.size();
    res.sockets.reserve(n);
    for (auto ix = decltype(n){0}; ix != n; ++ix) {
        if (sockets[ix].socket_id == main_core_socket_id) { // moving main core to last position in socket group
            auto i = std::next(begin(res.cores), offset);
            auto end_socket = std::next(i, sockets[ix].cores_n - 1); //if it last already we are done
            i = std::find(i, end_socket, main_core);
            if (i != end_socket)
                std::swap(*i, *end_socket);
            assert(*end_socket == main_core);
        }
        res.sockets.emplace_back(
            sockets[ix].socket_id,
            std::span(res.cores).subspan(offset, sockets[ix].cores_n)
        );
        offset += sockets[ix].cores_n;
    }
    assert(offset == res.cores.size());
    assert(std::ranges::is_sorted(res.sockets, std::ranges::greater{}, [](auto const& socket) {
        return socket.cores.size();
        }));
    return res;
}

void dpdkx::v0::config::adjust_to_socket(device& dev, sockets::socket const& socket) {
    auto const cores_n = static_cast<std::uint16_t>(socket.cores.size());
    assert(dev.info.default_rxportconf.nb_queues != 0);
    if (dev.info.default_rxportconf.nb_queues > cores_n)
        dev.info.default_rxportconf.nb_queues = cores_n;
    assert(dev.info.default_txportconf.nb_queues != 0);
    if (dev.info.default_txportconf.nb_queues > cores_n)
        dev.info.default_txportconf.nb_queues = cores_n;
    dev.socket_id = socket.socket_id;
}

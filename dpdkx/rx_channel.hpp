#pragma once
#include "netinet_in.hpp"
#include "rte_mbuf.h"
#include "rte_ip.h"
#include <memory>
#include <system_error>
#include <utility>
#include <cstddef>
#include <cstdint>

//#include "rte_mempool.h"


namespace dpdkx {

using queue_id_t = std::uint16_t;
using endpoint = std::pair<rte_be32_t, rte_be16_t>;
endpoint make_endpoint(sockaddr_in const& addr);

class rx_channel : public std::enable_shared_from_this<rx_channel> {
protected:
    struct use_make_rx_channel{};
public:
    rx_channel(use_make_rx_channel, class device& dev, sockaddr_in const& mcast_out_addr);
    virtual ~rx_channel() = default;
    constexpr auto& device() noexcept { return dev_; }
    template<typename T, typename ...Args>
    friend std::shared_ptr<T> make_rx_channel(class device& dev, sockaddr_in const& mcast_out_addr, Args&& ...args);
    virtual bool enqueue(queue_id_t queue_id, rte_ipv4_hdr* ipv4hdr, rte_mbuf* buffer);
    std::error_code join(sockaddr_in const& mcast_addr) { return join(make_endpoint(mcast_addr)); }
    std::error_code leave(sockaddr_in const& mcast_addr) { return leave(make_endpoint(mcast_addr)); }

    std::error_code join(endpoint ep);
    std::error_code leave(endpoint ep);

//  bool send(std::uint16_t qid, void const* payload, std::size_t payload_size);
private:
    class device& dev_;
};

template<typename T, typename ...Args>
std::shared_ptr<T> make_rx_channel(class device& dev, sockaddr_in const& addr, Args&& ...args) {
    if (addr.sin_family != AF_INET)
        throw std::system_error{ make_error_code(std::errc::address_family_not_supported) , "can't attach rx channel" };
    auto ch = std::make_shared<T>(rx_channel::use_make_rx_channel{}, dev, addr, std::forward<Args>(args)...);
    if(auto error = ch->join(addr))
        throw std::system_error{ error , "can't attach rx channel" };
    return ch;
}

inline std::shared_ptr<rx_channel> make_rx_channel(class device& dev, sockaddr_in const& mcast_out_addr) { return make_rx_channel<rx_channel>(dev, mcast_out_addr); }

} // namespace dpdkx



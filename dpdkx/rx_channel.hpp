#pragma once
#include "netinet_in.hpp"
#include "detail/dpdk_type.hpp"
#include "detail/proxy/mem_fn.hpp"
#include "rte_mbuf.h"
#include "rte_ip.h"
#include <memory>
#include <system_error>
#include <utility>
#include <cstddef>
#include <cstdint>


//#include "rte_mempool.h"


namespace dpdkx {


class rx_channel /*: public std::enable_shared_from_this<rx_channel>*/ {
public:
    rx_channel(class device& dev, sockaddr_in const& mcast_out_addr);
    virtual ~rx_channel() = default;
    constexpr auto& device() noexcept { return dev_; }
    /**
     * @return
     *   The number of packet has to be recycled from start of buffers .
     */
    virtual std::uint16_t enqueue(queue_id_t queue_id, rte_mbuf** buffers, std::uint16_t n);
    using enqueue_proxy_t = proxy::mem_fn<std::uint16_t(queue_id_t, rte_mbuf**, std::uint16_t)>;
    virtual enqueue_proxy_t enqueue_proxy() { throw std::runtime_error{"enqueue_proxy - not implemented"}; }
  private:
    class device& dev_;
};

ip4_endpoint make_endpoint(sockaddr_in const& addr);
std::error_code attach(class device& dev, ip4_endpoint ep, std::shared_ptr<rx_channel> channel);

template<typename T, typename ...Args>
[[deprecated]] std::shared_ptr<T> make_rx_channel(class device& dev, sockaddr_in const& addr, Args&& ...args) {
    if (addr.sin_family != AF_INET)
        throw std::system_error{ make_error_code(std::errc::address_family_not_supported) , "can't attach rx channel" };
    auto ch = std::make_shared<T>(dev, addr, std::forward<Args>(args)...);
    if (auto error = attach(dev, make_endpoint(addr), ch))
        throw std::system_error{ error , "can't attach rx channel" };
    return ch;
}

inline std::shared_ptr<rx_channel> make_rx_channel(class device& dev, sockaddr_in const& mcast_out_addr) { return make_rx_channel<rx_channel>(dev, mcast_out_addr); }

} // namespace dpdkx



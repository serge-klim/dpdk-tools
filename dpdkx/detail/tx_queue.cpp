#include "../device.hpp"
#include "tx_queue.hpp"
#include "error.hpp"
#include "rte_ip.h"
#include "rte_mbuf.h"
#include "rte_byteorder.h"
#include <algorithm>
#include <system_error>
#include <limits>
#include <cassert>

namespace {
auto make_ring_name(dpdkx::device& device, dpdkx::queue_id_t queue_id) {
    auto name = std::to_string(device.port_id());
    name += ':';
    name+= std::to_string(queue_id);
    return name;
}

}

dpdkx::tx_queue::tx_queue(dpdkx::device& device, queue_id_t queue_id, std::uint16_t ring_size /* = default_ring_size()*/)
: ring_ { rte_ring_create(make_ring_name(device,queue_id).c_str(), ring_size, static_cast<int>(device.socket_id()), RING_F_SC_DEQ | RING_F_MP_RTS_ENQ)} {
    if (!ring_)
        throw std::system_error{ dpdkx::last_error() , "unable to create: tx_ring_" + make_ring_name(device,queue_id) };
}

dpdkx::tx_queue::~tx_queue() noexcept 
{ 
    if(ring_)
        rte_ring_free(ring_); 
}

std::uint16_t dpdkx::tx_queue::enque(struct rte_mbuf** buffers, std::uint16_t n) noexcept {
    return /*rte_ring_enqueue_burst*/rte_ring_mp_enqueue_burst_elem(ring(), reinterpret_cast<void**>(buffers), sizeof(void*), n, nullptr);
}

bool dpdkx::tx_queue::enque(struct rte_mbuf* buffer) noexcept {
    return enque(&buffer, 1) !=0 ;
}

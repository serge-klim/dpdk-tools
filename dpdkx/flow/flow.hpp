#pragma once
#include "../detail/dpdk_type.hpp"
#include "rte_flow.h"
#include <utility>


namespace dpdkx::flow {

[[nodiscard]] rte_flow* make_udp_flow(port_id_t port, dpdkx::queue_id_t queue, ip4_endpoint endpoint, rte_flow_error& error) noexcept;
[[nodiscard]] rte_flow* make_udp_flow(port_id_t port, dpdkx::queue_id_t queue, ip4_endpoint endpoint);

} // namespace dpdkx::flow

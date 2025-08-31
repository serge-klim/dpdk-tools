#pragma once
#include "generic/rte_byteorder.h"
#include <utility>
#include <cstdint>


namespace dpdkx { inline namespace v0 { 

using port_id_t = std::uint16_t;
using queue_id_t = std::uint16_t;
using ip_port_t = rte_be16_t;
using core_t = unsigned int;

using ip4_endpoint = std::pair<rte_be32_t, rte_be16_t>;

}} // namespace dpdkx::v0


#pragma once
#include "rte_mempool.h"
#include <memory>

namespace dpdkx {

using scoped_mempool = std::unique_ptr<struct rte_mempool, decltype(&rte_mempool_free)>;
using shared_mempool = std::shared_ptr<struct rte_mempool>;
scoped_mempool make_scoped_mempool(const char* name, unsigned int n, unsigned int cache_size, uint16_t priv_size, uint16_t data_room_size,	int socket_id);

} // namespace dpdkx





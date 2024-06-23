#include "mempool.hpp"
#include "error.hpp"
#include "rte_mbuf.h"
#include <string>
#include <system_error>

dpdkx::scoped_mempool dpdkx::make_scoped_mempool(char const* name, unsigned int n, unsigned int cache_size, uint16_t priv_size, uint16_t data_room_size, int socket_id) {
	auto res = dpdkx::scoped_mempool{ rte_pktmbuf_pool_create(name, n, cache_size, priv_size, data_room_size, socket_id), &rte_mempool_free };
	if (!res)
		throw std::system_error{ dpdkx::last_error()/*dpdkx::make_error_code(ENOMEM)*/ , std::string{"unable to allocate : "} + (name ? name : "pktmbuf pool")};
	return res;
}






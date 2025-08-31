#include "log.hpp"
#include "rte_log.h"
#include "error.hpp"
#include <string>
#include <system_error>

int rtexx::detail::register_log_type_and_pick_level(const char* name, std::uint32_t loglevel)
{
	auto res = rte_log_register_type_and_pick_level(name, loglevel);
	if (res < 0)
		throw std::system_error{ dpdkx::last_error() , std::string{"unable register log : "} + name };
	return res;
}

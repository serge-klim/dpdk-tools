#include "log.hpp"
#include "rte_log.h"
#include "error.hpp"
#include <string>

rtexx::rte_logger::rte_logger(const char* name, std::uint32_t loglevel /*= RTE_LOG_INFO*/)
	:rte_logger{ rte_log_register_type_and_pick_level(name, loglevel) }
{
	if(logtype_ < 0)
		throw std::system_error{ dpdkx::last_error() , std::string{"unable register log : "} + name };
}

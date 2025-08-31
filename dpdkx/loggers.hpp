#pragma once
#include "log.hpp"
#include <type_traits>

namespace dpdkx { 
class device;
struct job;
class sink;
inline namespace v0 { namespace config {
 struct config_log; // tag
}}} // namespace dpdkx::v0::config 
 

namespace logging {

template<>
struct traits<dpdkx::config::config_log> {
    using type = rtexx::rte_logger;
};

#ifndef NDEBUG
using enable_debug_loggers = std::true_type;
#else
using enable_debug_loggers = std::false_type;
#endif // !NDEBUG

template<>
struct traits<dpdkx::job, std::true_type> {
    using type = rtexx::rte_logger;
};

template<>
struct traits<dpdkx::device> {
    using type = rtexx::rte_logger_ex<enable_debug_loggers::value ? rtexx::log_level::debug : rtexx::log_level::warning>;
};

template<>
struct traits<dpdkx::sink, enable_debug_loggers> {
    using type = rtexx::rte_logger;
};

template<>
logger_t<dpdkx::config::config_log>& logger<dpdkx::config::config_log>();


//template<>
//logger_t<dpdkx::device>& logger<dpdkx::device>();

} // namespace logging

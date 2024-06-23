#include "loggers.hpp"


template<>
logging::logger_t<dpdkx::config::config_log>& logging::logger<dpdkx::config::config_log>() {
    static auto res = rtexx::rte_logger{ "dpdkx::device" };
    return res;
}

//template<>
//logging::logger_t<dpdkx::device>& logging::logger<dpdkx::device>() {
//    static auto res = rtexx::rte_logger{ "dpdkx::device" };
//    return res;
//}

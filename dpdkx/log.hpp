#pragma once
#include "rte_log.h"
#include <version>
#if defined (__cpp_lib_format)
	#include <format>
	namespace fmt=std;
#else
	#include "fmt/format.h"
#endif
#include <vector>
#include <iterator>
#include <cstdint>
#include <string>
#include <type_traits>
#include <boost/type_index.hpp>




namespace rtexx {


enum class log_level : std::uint32_t {
	emergancy = RTE_LOG_EMERG,
	alert = RTE_LOG_ALERT,
	critical = RTE_LOG_CRIT,
	error = RTE_LOG_ERR,
	warning = RTE_LOG_WARNING,
	notice = RTE_LOG_NOTICE,
	info = RTE_LOG_INFO,
	debug = RTE_LOG_DEBUG
};


template<typename Format, typename ...Args>
void log(int logtype, log_level level, Format&& fmt,  Args&& ...args) {

	if (!rte_log_can_log(static_cast<std::uint32_t>(logtype), static_cast<std::uint32_t>(level)))
		return;

	auto out = rte_log_get_stream();
	///* save loglevel and logtype in a global per-lcore variable */
	//RTE_PER_LCORE(log_cur_msg).loglevel = level;
	//RTE_PER_LCORE(log_cur_msg).logtype = logtype;

	static thread_local auto buffer = [] {
		auto res = std::vector<char>{};
		res.reserve(512);
		return res;
		}();
	buffer.clear();
	try {
		*fmt::vformat_to(std::back_inserter(buffer), std::forward<Format>(fmt), fmt::make_format_args(args...)) = '\n';
	}catch(fmt::format_error const& error) {
		auto msg = std::string{ "unable format log entry : " } + error.what();
		buffer.assign(begin(msg), end(msg));
	}
	//buffer.push_back('\0');
	//rte_log(static_cast<std::uint32_t>(level), logtype, buffer.data());
	fwrite(buffer.data(), sizeof(char), buffer.size(), out);
    if(static_cast<std::uint32_t>(level) <= static_cast<std::uint32_t>(log_level::warning))
        fflush(out); 
}

class rte_logger
{
public:
	constexpr rte_logger(int logtype) noexcept : logtype_{ logtype } {}
	explicit rte_logger(char const* name, std::uint32_t loglevel = RTE_LOG_INFO);
	explicit rte_logger(std::string const& name, std::uint32_t loglevel = RTE_LOG_INFO) : rte_logger{name.c_str(), loglevel} {}

	template<typename ...Args>
	void log(log_level level, Args&& ...args) { rtexx::log(logtype_, level, std::forward<Args>(args)...); }
	template <typename... Args>
	 void trace(Args&&...args)  { log(log_level::debug, std::forward<Args>(args)...); }
	template <typename... Args>
	 void debug(Args&&...args)  { log(log_level::debug, std::forward<Args>(args)...); }
	template <typename... Args>
	void info(Args&&...args)  { log(log_level::info, std::forward<Args>(args)...); }
	template <typename... Args>
	 void warn(Args&&...args)  { log(log_level::warning, std::forward<Args>(args)...); }
	template <typename... Args>
	 void error(Args&&...args)  { log(log_level::error, std::forward<Args>(args)...); }
	template <typename... Args>
	 void critical(Args&&...args)  { log(log_level::critical, std::forward<Args>(args)...); }


	template <typename... Args>
	 void alert(Args&&...args)  { log(log_level::alert, std::forward<Args>(args)...); }
	template <typename... Args>
	 void emergency(Args&&...args)  { log(log_level::emergancy, std::forward<Args>(args)...); }
private:
	int logtype_;
};

} // namespace rtex

namespace logging {

struct null_logger
{
	null_logger() = default;
	template <typename... Args>
	constexpr null_logger(Args&&...) noexcept {}
	template <typename... Args>
	constexpr void log(Args&&...) const noexcept {}
	template <typename... Args>
	constexpr void trace(Args&&...) const noexcept {}
	template <typename... Args>
	constexpr void debug(Args&&...) const noexcept {}
	template <typename... Args>
	constexpr void info(Args&&...) const noexcept {}
	template <typename... Args>
	constexpr void warn(Args&&...) const noexcept {}
	template <typename... Args>
	constexpr void error(Args&&...) const noexcept {}
	template <typename... Args>
	constexpr void critical(Args&&...) const noexcept {}

	//constexpr void set_level(log_level /*level*/) const noexcept {}
	//[[nodiscard]] level log_level() const;
	//// return the name of the logger
	//[[nodiscard]] const std::string& name() const;
};

template<typename T, typename Enable = std::true_type>
struct traits{
	using type = null_logger;
};

template<typename T>
using logger_t = typename traits<T>::type;

template<typename T>
std::string logger_name() { return boost::typeindex::type_id<T>().pretty_name(); }


template<typename T>
logger_t<T>& logger() {
	static auto res = logger_t<T>{ logger_name<T>() };
	return res;
}

} // namespace logging


#pragma once
#include "rte_eal.h"
#include "rte_cycles.h"
#include <utility>
#include <chrono>
#include <string>
#include <vector>

namespace dpdkx {

template<typename Rep, typename Period>
[[nodiscard]] std::pair<std::chrono::milliseconds, int> retry(std::chrono::duration<Rep, Period> timeout)
{
    constexpr static auto default_timeout = std::chrono::milliseconds{ 100 /* 100ms */ };
    auto res = std::make_pair(std::chrono::duration_cast<std::chrono::milliseconds>(timeout), 1);
    if (res.first <= default_timeout)
        return res;
    return { default_timeout,
             (res.first + default_timeout - std::chrono::milliseconds{ 1 }) / default_timeout
    };
}

[[nodiscard]] inline std::uint64_t/*rte_mbuf_timestamp_t*/ ns_timer() noexcept {
    return rte_get_timer_cycles() * NS_PER_S/*MS_PER_S*/ / rte_get_timer_hz();
}

struct cleanup{
    void operator()(void const*) const noexcept { rte_eal_cleanup(); }
};

using scoped_eal = std::unique_ptr<void, cleanup>;
[[nodiscard]] scoped_eal eal_init(std::vector<std::string> const& eal_strings);

} // namespace dpdkx


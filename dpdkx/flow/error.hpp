#pragma once
#include "rte_flow.h"
#include <system_error>


namespace dpdkx::flow {
	

const std::error_category& error_category() noexcept;
    
[[nodiscard]] inline std::error_code make_error_code(int error) noexcept {
	return { error, error_category() };
}

[[nodiscard]] rte_flow_error const& last_error() noexcept;
    
[[nodiscard]] inline std::error_code last_error_code() noexcept {
   return {static_cast<int>(last_error().type), error_category()};
}

namespace detail {
rte_flow_error& last_error() noexcept;
}

} // namespace dpdkx::flow


#pragma once
#include "rte_errno.h"
#include <system_error>
 


namespace dpdkx {
	
enum class errc {
   no_avalible_queue = RTE_MAX_ERRNO
};

const std::error_category& error_category() noexcept;
    
[[nodiscard]] inline std::error_code make_error_code(int error) noexcept {
	return { error, error_category() };
}

[[nodiscard]] inline std::error_code make_error_code(errc error) noexcept {
   return {static_cast<int>(error), error_category()};
}

[[nodiscard]] std::error_code last_error() noexcept;
    
} // namespace dpdkx


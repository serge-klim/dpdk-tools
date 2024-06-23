#pragma once
#include <system_error>


namespace dpdkx {
	

const std::error_category& error_category() noexcept;
    
[[nodiscard]] inline std::error_code make_error_code(int error) noexcept {
	return { error, error_category() };
}

[[nodiscard]] std::error_code last_error() noexcept;
    

} // namespace dpdkx


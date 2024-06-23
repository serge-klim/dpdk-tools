#include "utils.hpp"
#include "error.hpp"
#include "utils/workarounds.hpp"
#include <ranges>
#include <functional>

dpdkx::scoped_eal dpdkx::eal_init(std::vector<std::string> const& eal_strings) {
    auto eal_options = utils::workarounds::to<std::vector<char const*>>(eal_strings | std::views::transform(std::mem_fn(&std::string::c_str)))/* | std::ranges::to<std::vector<char const*>>()*/;
    if (auto error = rte_eal_init(static_cast<int>(eal_options.size()), const_cast<char**>(eal_options.data())); error < 0)
        throw std::runtime_error{ std::string{"rte_eal_init failed : "} + std::to_string(error) };
    return dpdkx::scoped_eal{ &eal_options};
}
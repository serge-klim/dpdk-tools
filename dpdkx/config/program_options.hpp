#pragma once
#include "device.hpp"
#include "utils/nic_addresses.hpp"
#include "rte_ether.h"
#include <boost/program_options.hpp>
#include <optional>
#include <vector>
#include <string>
#include <span>


namespace dpdkx { inline namespace v0 { namespace config {
 struct config_log; // tag

struct device_matcher {
public:
    device_matcher(std::string id);
    constexpr std::string const& id() const noexcept { return id_; }
    bool operator () (utils::net::nic_info const& info) const;
    bool operator () (rte_ether_addr const& mac_addr) const;
    bool operator () (device const& dev) const;
private:
    std::string id_;
    std::vector<unsigned char> mac_;
    std::optional<in_addr> addr_;
};

[[nodiscard]] std::vector<utils::net::nic_info> get_nic_info(boost::program_options::variables_map const& options);
[[nodiscard]] std::vector<device> devices_configuration(std::vector<utils::net::nic_info> const& nics_info, boost::program_options::variables_map const& options = {});
[[nodiscard]] std::vector<std::string> eal_option(std::string app_name, boost::program_options::variables_map const& options);

void apply_workarounds(std::span<device> devices, boost::program_options::variables_map const& options = {});

//std::pair<unsigned int, std::uint16_t> mbuf_pool_config(device const& dev, boost::program_options::variables_map const& options);
}}} // namespace dpdkx::v0::config


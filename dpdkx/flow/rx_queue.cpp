#include "loggers.hpp"
#include "rx_queue.hpp"
#include "flow.hpp"
#include "error.hpp"
#include "../device.hpp"
#include <algorithm>
#include <format>
#include <stdexcept>
#include <cassert>

dpdkx::flow::rx_queue::rx_queue(config::device const& config /*port_id_t port*/, queue_id_t queue_id)
    : port_id_{config.port_id}, queue_id_{queue_id}, buffers_(config.info.default_rxportconf.burst_size) {
}

dpdkx::flow::rx_queue::rx_queue(rx_queue&& other)
    : port_id_{other.port_id_}, queue_id_{other.queue_id_}, flows_{std::move(other.flows_)}, buffers_{std::move(other.buffers_)} {
   if (workaround::load(other.ch_))
      throw std::logic_error{"can't move flow::rx_queue with assigned channel"};
}

dpdkx::flow::rx_queue::~rx_queue() {
   reset();
}

void dpdkx::flow::rx_queue::start() {
   if (flows_.empty()) {
      auto& flow_error = detail::last_error();
      auto flow = make_udp_flow(port_id_, queue_id_, flow_stub_endpoint(queue_id_), flow_error);
      if (!flow) {
         auto error = last_error_code();
         logging::logger<dpdkx::device>().warn("unable to create default flow for port : {}[{}] : {} ", port_id_, queue_id_, error.message());
         throw std::system_error{error, std::format("unable to start queue {} port {}, can't create default flow.", queue_id_, port_id_)};
      }
      flows_.emplace_back(flow);
   }
}

void dpdkx::flow::rx_queue::reset() {
   const auto [begin, end] = std::ranges::remove_if(flows_, [this](auto& flow) {
      auto flow_error = rte_flow_error{};
      auto res = rte_flow_destroy(port_id_, flow, &flow_error) == 0;
      if (!res)
         logging::logger<dpdkx::device>().warn("unable to destroy flow for port : {}[{}] {} {}", port_id_, queue_id_, static_cast<int>(flow_error.type), flow_error.message);
      return res;
   });
   flows_.erase(begin, end);
}

void dpdkx::flow::rx_queue::reset(std::shared_ptr<rx_channel> ch, ip4_endpoint ep) {
   reset();
   for (auto n = flows_.size(); n-- != 0;) {
      auto& flow_error = detail::last_error();
      if (rte_flow_destroy(port_id_, flows_[n], &flow_error) != 0)
         throw std::system_error{last_error_code(), std::format("unable to delete flow for port : {}[{}]", port_id_, queue_id_)};
      flows_.resize(n);
   }
   assert(flows_.empty());
   workaround::store(ch_, std::move(ch));
   if(auto error = route_to(ep))
      throw std::system_error{error, std::format("unable to make flow for port : {} : {}", port_id_, queue_id_)};
}

std::error_code dpdkx::flow::rx_queue::route_to(ip4_endpoint ep) {
   auto flow = make_udp_flow(port_id_, queue_id_, ep, detail::last_error());
   if (!flow)
      return last_error_code();
   flows_.push_back(flow);
   return {};
}

dpdkx::job_state dpdkx::flow::rx_queue::process() {
   assert("not implemented yet");
   throw std::runtime_error{"not implemented yet"};
}



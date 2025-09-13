#pragma once
#include "../detail/proxy/mem_fn.hpp"
#include "../detail/dpdk_type.hpp"
#include "../jobs.hpp"
#include "../rx_channel.hpp"
#include "../config/device.hpp"
#include "../detail/atomic_shared_ptr.hpp"
#include "rte_flow.h"
#include <vector>
#include <utility>


namespace dpdkx::flow {

constexpr ip4_endpoint flow_stub_endpoint(queue_id_t queue_id) noexcept { return {0xe0 | rte_cpu_to_be_32(queue_id), 0}; }

class rx_queue : public job {
 public:
   rx_queue(config::device const& config/*port_id_t port*/, queue_id_t queue_id);
   ~rx_queue();
   rx_queue(rx_queue const&) = delete;
   rx_queue& operator= (rx_queue const&) = delete;
   rx_queue(rx_queue&&);
   rx_queue& operator=(rx_queue&&) = delete; // due to atomic_shared_ptr<rx_channel>
   bool assigned() const noexcept { return static_cast<bool>(workaround::load(ch_)); }
   std::shared_ptr<rx_channel> channel() noexcept { return workaround::load(ch_); }
   constexpr std::pair<port_id_t, queue_id_t> device() const noexcept { return {port_id_, queue_id_}; }
   void reset(std::shared_ptr<rx_channel> ch, ip4_endpoint ep);
   std::error_code route_to(ip4_endpoint ep);
   void start();
   [[nodiscard]] job_state process() override;
   int run_exclusive() noexcept override;
 private:
   void reset();
 private:
   port_id_t port_id_;
   queue_id_t queue_id_;
   std::vector<rte_flow*> flows_;
   std::vector<rte_mbuf*> buffers_;
   workaround::atomic_shared_ptr<rx_channel> ch_; 
};

static_assert(!std::is_copy_constructible_v<rx_queue>, "flow::rx_queue copy constructor should be disabled");
static_assert(!std::is_copy_assignable_v<rx_queue>, "flow::rx_queue shouldn't be copy assignable ");

} // namespace dpdkx::flow

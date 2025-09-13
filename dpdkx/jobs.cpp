#include "jobs.hpp"
#include "flow/rx_queue.hpp"
#include "device.hpp"
#include "loggers.hpp"
#include "error.hpp"
#include "detail/utils.hpp"
#include "rte_ip_frag.h"
#include "rte_lcore.h"
#include <chrono>
#include <exception>
#include <atomic>

namespace {
static auto cycle_threshold = std::uint16_t{33};
static auto idle_threshold = std::uint8_t{2};
auto running = std::atomic<bool>{true};
}

bool dpdkx::jobs_suspended() noexcept {
    return !running.load(std::memory_order_relaxed);
}

void dpdkx::stop_jobs() noexcept {
    running = false;
}

dpdkx::job_sentry::~job_sentry() {
    logging::logger<dpdkx::job>().info("stopping all jobs...");
    stop_jobs();
    rte_eal_mp_wait_lcore();
}


int dpdkx::job::run_exclusive() noexcept {
    auto const lcore_id = rte_lcore_id();
    logging::logger<dpdkx::job>().info("running single job on core : {}", lcore_id);
    try {
       auto idling = decltype(idle_threshold){0};
       while (running.load(std::memory_order_relaxed)) {
          for (auto cycle = decltype(cycle_threshold){0}; cycle < cycle_threshold; ++cycle) {
             switch (process()) {
                [[likely]] case job_state::busy:
                   idling = 0;
                   break;
                [[unlikely]] case job_state::idling:
                   if (++idling > idle_threshold)
                      cycle = cycle_threshold;
                   break;
                [[unlikely]] case job_state::done:
                   return 0;
             }
          }
       }
    } catch (std::exception& e) {
        logging::logger<dpdkx::job>().error("job failed on lcore - {} : {}", lcore_id, e.what());
    } catch (...) {
        logging::logger<dpdkx::job>().error("job unexpectedly failed on lcore - {}", lcore_id);
    }
    return 0;
}

dpdkx::job_state dpdkx::run_jobs_once(std::vector<dpdkx::job*>& jobs) {
    auto res = job_state::idling;
    auto end = std::end(jobs);
    for (auto job = begin(jobs); job != end; ) {
        switch ((*job)->process()) {
            [[unlikely]] case job_state::idling:
                break;
            [[likely]] case job_state::busy:
                res = job_state::busy;
                break;
            [[unlikely]] case job_state::done:
            {
                job = jobs.erase(job);
                if(jobs.empty())
                    return job_state::done;
                end = std::end(jobs);
                continue;
            }
        }        
        ++job;
    }
    return res;
}

int dpdkx::run_jobs(void* param) noexcept {
    auto const lcore_id = rte_lcore_id();
    auto jobs = static_cast<core_jobs*>(param)->jobs;
    auto const n = jobs.size();
    if (n == 1)
        return jobs.front()->run_exclusive();

    logging::logger<dpdkx::job>().info("running {} jobs on core : {}", n, lcore_id);
    assert(static_cast<core_jobs*>(param)->core_id == lcore_id);
    try {
        auto idling = decltype(idle_threshold){0};
        //std::erase_if(jobs, std::not_fn(std::mem_fn(&job::resume)));
        while (running.load(std::memory_order_relaxed)) {
           for (auto cycle = decltype(cycle_threshold){0}; cycle < cycle_threshold; ++cycle) {
              assert(!jobs.empty());
              switch (run_jobs_once(jobs)) {
                 [[likely]] case job_state::busy:
                    idling = 0;
                    break;
                 [[unlikely]] case job_state::idling:
                    if (++idling > idle_threshold)
                       cycle = cycle_threshold;
                    break;
                 [[unlikely]] case job_state::done:
                    return 0;
              }
           }
        }
    } catch (std::exception& e) {
        logging::logger<dpdkx::job>().error("jobs runner failed on lcore - {} : {}", lcore_id, e.what());
    } catch (...) {
        logging::logger<dpdkx::job>().error("jobs runner unexpectedly failed on lcore - {}", lcore_id);
    }
    return 0;
}

int dpdkx::flow::rx_queue::run_exclusive() noexcept {
   auto const lcore_id = rte_lcore_id();
   logging::logger<dpdkx::job>().info("running {} flow::rx_queue {} exclusively on core : {}", port_id_, queue_id_, lcore_id);
   try {
      while (!assigned()) {
          if (!running.load(std::memory_order_relaxed))
             return 0;
         // if (const auto n = rte_eth_rx_burst(port_id_, queue_id_, buffers_.data(), buffers_.size())) {
         //    assert("flow::rx_queue:received unexpected pakets");
         //    logging::logger<dpdkx::job>().info("received unexpected pakets  on {} flow::rx_queue {} exclusively on core : {}", port_id_, queue_id_, lcore_id);
         // }
         rte_delay_ms(500u);
      }
      auto channel = workaround::load(ch_);
      assert(channel != nullptr);
      auto proxy = channel->enqueue_proxy();
      while (running.load(std::memory_order_relaxed)) {
         if (const auto n = rte_eth_rx_burst(port_id_, queue_id_, buffers_.data(), buffers_.size())) {
            for (auto i = static_cast<decltype(n)>(0); i != n; ++i) {
               update_l2size(buffers_[i]);
               auto ipv4hdr = rte_pktmbuf_mtod_offset(buffers_[i], rte_ipv4_hdr*, buffers_[i]->l2_len);
               // assert(rte_ipv4_hdr_len(ipv4hdr) <= sizeof(*ipv4hdr));
               buffers_[i]->l3_len = rte_ipv4_hdr_len(ipv4hdr) /*sizeof(*ipv4hdr)*/;
               assert(ipv4hdr->next_proto_id == IPPROTO_UDP);
               assert(!rte_ipv4_frag_pkt_is_fragmented(ipv4hdr));
               // auto udphdr = rte_pktmbuf_mtod_offset(buffers_[i], rte_udp_hdr*, buffers_[i]->l2_len + buffers_[i]->l3_len);
               buffers_[i]->l4_len = sizeof(struct rte_udp_hdr);
            }
            proxy(queue_id_, buffers_.data(), n);
         }
      }
   } catch (std::exception& e) {
      logging::logger<dpdkx::job>().error("{} flow::rx_queue {} failed on lcore - {} : {}", port_id_, queue_id_, lcore_id, e.what());
   } catch (...) {
      logging::logger<dpdkx::job>().error("{} flow::rx_queue {} unexpectedly failed on lcore - {}", port_id_, queue_id_, lcore_id);
   }
   return 0;
}



std::error_code dpdkx::ether_address(class device& device, rte_be32_t ip4addr, rte_ether_addr& mac_addr, std::vector<job*>& jobs, std::chrono::milliseconds timeout) noexcept {
    auto static const not_supported = make_error_code(std::errc::not_supported);
    auto start = std::chrono::system_clock::now();
    do {
        auto res = device.ether_address(ip4addr, mac_addr);
        if (!res || res == not_supported)
            return res;
        if (unlikely(run_jobs_once(jobs) == job_state::done))
            return make_error_code(std::errc::connection_aborted);
        if (unlikely(!running.load(std::memory_order_relaxed)))
            return make_error_code(std::errc::operation_canceled);
    } while (std::chrono::system_clock::now() - start < timeout);
    return make_error_code(std::errc::timed_out);
}

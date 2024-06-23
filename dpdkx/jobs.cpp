#include "jobs.hpp"
#include "device.hpp"
#include "loggers.hpp"
#include "error.hpp"
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

int dpdkx::run_single_job(void* param) noexcept {
    auto const lcore_id = rte_lcore_id();
    logging::logger<dpdkx::job>().info("running single job on core : {}", lcore_id);
    try {
        auto idling = decltype(idle_threshold){0};
        for (auto cycle = decltype(cycle_threshold){0};;++cycle) {
            switch(static_cast<dpdkx::job*>(param)->process()) 
            {
                case job_state::idling:
                    ++idling;
                    break;
                case job_state::busy:
                    idling = 0;
                    break;
                case job_state::done:
                    return 0;
            }
            if (unlikely(cycle > cycle_threshold || idling > idle_threshold)) {
                cycle = 0;
                if (!running.load(std::memory_order_relaxed))
                    break;
            }
        }
    }
    catch (std::exception& e) {
        logging::logger<dpdkx::job>().error("job failed on lcore - {} : {}", lcore_id, e.what());
    }
    catch (...) {
        logging::logger<dpdkx::job>().error("job unexpectedly failed on lcore - {}", lcore_id);
    }
    return 0;
}

dpdkx::job_state dpdkx::run_jobs_once(std::vector<dpdkx::job*>& jobs) {
    auto res = job_state::idling;
    auto end = std::end(jobs);
    for (auto job = begin(jobs); job != end; ) {
        switch ((*job)->process())
        {
            case job_state::idling:
                break;
            case job_state::busy:
                res = job_state::busy;
                break;
            case job_state::done:
            {
                job = jobs.erase(job);
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
        return run_single_job(jobs.front());

    logging::logger<dpdkx::job>().info("running {} jobs on core : {}", n, lcore_id);
    assert(static_cast<core_jobs*>(param)->core_id == lcore_id);
    try {
        auto idling = decltype(idle_threshold){0};
        //std::erase_if(jobs, std::not_fn(std::mem_fn(&job::resume)));
        for (auto cycle = decltype(cycle_threshold){0}; !jobs.empty(); ++cycle) {
            switch(run_jobs_once(jobs))
            {
                case job_state::idling:
                    ++idling;
                    break;
                case job_state::busy:
                    idling = 0;
                    break;
                case job_state::done:
                    return 0;
            }
            if (unlikely(cycle > cycle_threshold || idling > idle_threshold)) {
                cycle = 0;
                if (!running.load(std::memory_order_relaxed))
                    break;
            }
        }
    }
    catch (std::exception& e) {
        logging::logger<dpdkx::job>().error("jobs runner failed on lcore - {} : {}", lcore_id, e.what());
    }
    catch (...) {
        logging::logger<dpdkx::job>().error("jobs runner unexpectedly failed on lcore - {}", lcore_id);
    }
    return 0;
}


std::error_code dpdkx::ether_address(class device& device, rte_be32_t ip4addr, rte_ether_addr& mac_addr, std::vector<job*>& jobs, std::chrono::milliseconds timeout) noexcept {
    auto const not_supported = make_error_code(std::errc::not_supported);
    auto start = std::chrono::system_clock::now();
    do {
        auto res = device.ether_address(ip4addr, mac_addr);
        if (!res || res == not_supported)
            return res;
        run_jobs_once(jobs);
        if (!running.load(std::memory_order_relaxed))
            return make_error_code(std::errc::operation_canceled);
        if(unlikely(jobs.empty()))
            return make_error_code(std::errc::connection_aborted);
    } while (std::chrono::system_clock::now() - start < timeout);
    return make_error_code(std::errc::timed_out);
}

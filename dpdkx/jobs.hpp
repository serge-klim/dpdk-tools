#pragma once
#include "rte_ether.h"
#include <chrono>
#include <vector>
#include <system_error>


namespace dpdkx {
enum class job_state {
    busy,
    idling,
    done
};

bool jobs_suspended() noexcept;
void stop_jobs() noexcept;

struct job {
    //virtual bool resume() { return true; }
    [[nodiscard]] virtual job_state process() = 0;
    //virtual bool idle(std::size_t n) { return true; };
    //virtual void process(struct rte_mbuf* buffers, std::size_t size) = 0;
};

int run_single_job(void* param) noexcept;
[[nodiscard]] job_state run_jobs_once(std::vector<dpdkx::job*>& jobs);

struct core_jobs {
    unsigned int core_id;
    std::vector<dpdkx::job*> jobs;
};
int run_jobs(void* param) noexcept;

std::error_code ether_address(class device& device, rte_be32_t ip4addr, rte_ether_addr& mac_addr, std::vector<struct job*>& jobs, std::chrono::milliseconds timeout = std::chrono::duration_cast<std::chrono::milliseconds>(std::chrono::seconds{20})) noexcept;

struct job_sentry {
    job_sentry() = default;
    job_sentry(job_sentry const&) = delete;
    job_sentry& operator=(job_sentry const&) = delete;
    job_sentry(job_sentry&&) = delete;
    job_sentry& operator=(job_sentry&&) = delete;
    ~job_sentry();
};

} // namespace dpdkx




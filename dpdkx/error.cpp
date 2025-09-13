#include "error.hpp"
#include "rte_errno.h"
#include <string>

namespace {
struct dpdk_error_category : std::error_category {

   char const* name() const noexcept override { return "dpdk error"; }

   std::string message(int code) const override {
      // E_RTE_NO_CONFIG - function could not get pointer to rte_config structure
      // EINVAL - cache size provided is too large, or priv_size is not aligned.
      // ENOSPC - the maximum number of memzones has already been allocated
      // EEXIST - a memzone with the same name already exists
      // ENOMEM - no appropriate memory area found in which to create memzone
      if (code > RTE_MAX_ERRNO)
         return message(static_cast<dpdkx::errc>(code));
      return std::string{"dpdk error : "} + rte_strerror(code) + " [" + std::to_string(code) + ']';
   }

   std::string message(dpdkx::errc error) const {
      switch (error) {
         case dpdkx::errc::no_avalible_queue:
            return "no queue avalible";
      }
      return "unknow error code : " + std::to_string(static_cast<int>(error));
   }
};

} // namespace

const std::error_category& dpdkx::error_category() noexcept {
   static auto const res = dpdk_error_category{};
   return res;
}

std::error_code dpdkx::last_error() noexcept {
   return make_error_code(rte_errno);
}

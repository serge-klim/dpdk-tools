#include "error.hpp"
#include "rte_flow.h"
#include <string>

namespace {

constexpr std::string to_string(rte_flow_error_type error) noexcept {
   switch (error) {
      case RTE_FLOW_ERROR_TYPE_NONE: return "No error.";
      case RTE_FLOW_ERROR_TYPE_UNSPECIFIED: return "Cause unspecified.";
      case RTE_FLOW_ERROR_TYPE_HANDLE: return "Flow rule (handle).";
      case RTE_FLOW_ERROR_TYPE_ATTR_GROUP: return "Group field.";
      case RTE_FLOW_ERROR_TYPE_ATTR_PRIORITY: return "Priority field.";
      case RTE_FLOW_ERROR_TYPE_ATTR_INGRESS: return "Ingress field.";
      case RTE_FLOW_ERROR_TYPE_ATTR_EGRESS: return "Egress field.";
      case RTE_FLOW_ERROR_TYPE_ATTR_TRANSFER: return "Transfer field.";
      case RTE_FLOW_ERROR_TYPE_ATTR: return "Attributes structure.";
      case RTE_FLOW_ERROR_TYPE_ITEM_NUM: return "Pattern length.";
      case RTE_FLOW_ERROR_TYPE_ITEM_SPEC: return "Item specification.";
      case RTE_FLOW_ERROR_TYPE_ITEM_LAST: return "Item specification range.";
      case RTE_FLOW_ERROR_TYPE_ITEM_MASK: return "Item specification mask.";
      case RTE_FLOW_ERROR_TYPE_ITEM: return "Specific pattern item.";
      case RTE_FLOW_ERROR_TYPE_ACTION_NUM: return "Number of actions.";
      case RTE_FLOW_ERROR_TYPE_ACTION_CONF: return "Action configuration.";
      case RTE_FLOW_ERROR_TYPE_ACTION: return "Specific action.";
      case RTE_FLOW_ERROR_TYPE_STATE: return "Current device state.";
   }
   return "unknow error : " + std::to_string(static_cast<int>(error));
}

struct dpdk_flow_error_category : std::error_category {

   char const* name() const noexcept override { return "dpdk flow error"; }

   std::string message(int code) const override {
      auto msg = std::string{"dpdk flow error : "};
      msg += to_string(static_cast<rte_flow_error_type>(code));
      if (dpdkx::flow::last_error().type == static_cast<rte_flow_error_type>(code)) {
         if (auto m = dpdkx::flow::last_error().message) {
            msg += " - ";
            msg += m;
         }
      }
      return msg;
   }
};

} // namespace

const std::error_category& dpdkx::flow::error_category() noexcept {
   static auto const res = dpdk_flow_error_category{};
   return res;
}

rte_flow_error const& dpdkx::flow::last_error() noexcept {
   return detail::last_error();
}

rte_flow_error& dpdkx::flow::detail::last_error() noexcept {
   static thread_local auto res = rte_flow_error{RTE_FLOW_ERROR_TYPE_NONE, nullptr, nullptr};
   return res;
}
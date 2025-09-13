#include "flow.hpp"
#include "device.hpp"
#include "rte_ethdev.h"
#include <format>


rte_flow* dpdkx::flow::make_udp_flow(port_id_t port, dpdkx::queue_id_t queue, ip4_endpoint endpoint) {
   rte_flow_error error;
   auto res = make_udp_flow(port, queue, endpoint, error);
   if (!res)
      throw std::runtime_error{std::format("unable create flow, error : {} {} {}", static_cast<int>(error.type), error.cause, error.message)};
   return res;
}

rte_flow* dpdkx::flow::make_udp_flow(port_id_t port, dpdkx::queue_id_t queue, ip4_endpoint endpoint, rte_flow_error& error) noexcept {
   auto ip4_spec = rte_flow_item_ipv4{
       .hdr = {
           .next_proto_id = IPPROTO_UDP,
           .dst_addr = endpoint.first
       }};
   auto ip4_mask = rte_flow_item_ipv4{
       .hdr = {
           .next_proto_id = 0xff,
           .dst_addr = 0xffffffff
       }};
    
   auto udp_spec = rte_flow_item_udp{.hdr = {.dst_port = endpoint.second /*ntohs(port)*/}};
   auto udp_mask = rte_flow_item_udp{.hdr = {.dst_port = 0xffff}};

   rte_flow_item pattern[] = {
       {.type = RTE_FLOW_ITEM_TYPE_ETH},  /* pass all eth packets */
       {.type = RTE_FLOW_ITEM_TYPE_IPV4, .spec = &ip4_spec, .mask = &ip4_mask}, /* pass all ipv4 packets */
       /* match source port udp */
       {.type = RTE_FLOW_ITEM_TYPE_UDP, .spec = &udp_spec, .mask = &udp_mask},
       {.type = RTE_FLOW_ITEM_TYPE_END}
   };
   /* queue to send packet */
   struct rte_flow_action_queue queue_action = {.index = queue};

   /* create the queue action */
   struct rte_flow_action actions[] = {
       {.type = RTE_FLOW_ACTION_TYPE_QUEUE,.conf = &queue_action},
       {.type = RTE_FLOW_ACTION_TYPE_END}
   };

   struct rte_flow_attr attr = {.ingress = 1};
   return rte_flow_create(port, &attr, pattern, actions, &error);
}


//void setup_flow(dpdkx::device const& device, rte_ether_addr const& mac) {
//    /* create the attribute structure */
//    // tpmd > flow create 0 ingress pattern eth / udp / end actions drop / end
// 
//    auto pattern = std::array<rte_flow_item, 4>{};
//    struct rte_flow_item_eth eth = { 0 };
//    eth.type = RTE_BE16(RTE_ETHER_TYPE_IPV4);
//    eth.hdr.dst_addr = mac;
//    eth.hdr.src_addr = device.mac_addr();
//    pattern[0].type = RTE_FLOW_ITEM_TYPE_ETH;
//    pattern[0].spec = &eth;
//    pattern[0].mask = &eth;
//
//    // ///* set the dst ipv4 packet to the required value */
//    auto ipv4 = rte_flow_item_ipv4{0};
//    //ipv4.hdr.dst_addr = htonl(0xc0a80302);
//    ipv4.hdr.next_proto_id = RTE_BE16(IPPROTO_UDP);
//    pattern[1].type = RTE_FLOW_ITEM_TYPE_IPV4;
//    pattern[1].spec = &ipv4;
//    pattern[1].mask = &ipv4;
//
//    // auto udp = rte_flow_item_udp{0};
//    // /* set the udp to pas all packets */
//    // pattern[2].type = RTE_FLOW_ITEM_TYPE_UDP;
//    // pattern[2].spec = &udp;
//
//    /* end the pattern array */
//    pattern[2].type = RTE_FLOW_ITEM_TYPE_END;
//
//    auto actions = std::array<rte_flow_action, 2>{};
//    ///* create the drop action */
//    //actions[0].type = RTE_FLOW_ACTION_TYPE_DROP;
//    struct rte_flow_action_queue queue = { .index = 0/*rx_q*/ };
//    actions[0].type = RTE_FLOW_ACTION_TYPE_QUEUE;
//    actions[0].conf = &queue;
//
//    actions[1].type = RTE_FLOW_ACTION_TYPE_END;
//
//    struct rte_flow_attr attr = { .ingress = 1 };
//    /* validate and create the flow rule */
//    //struct rte_flow* flow;
//    struct rte_flow_error error;
//    auto flow = rte_flow_create(device.port_id(), &attr, pattern.data(), actions.data(), &error);
//    if (!flow) {
//        // let's make it less specific connectx on windows seems not support destination
//        std::memset(&eth.dst, 0, sizeof(eth.dst));
//        flow = rte_flow_create(device.port_id(), &attr, pattern.data(), actions.data(), &error);
//        if(!flow){
//            std::memset(&eth.src, 0, sizeof(eth.dst));
//            pattern[1].type = RTE_FLOW_ITEM_TYPE_END;
//            flow = rte_flow_create(device.port_id(), &attr, pattern.data(), actions.data(), &error);
//            if(!flow)
//                BOOST_LOG_SEV(log::get(), boost::log::trivial::warning) << "unable create flow : " << error.message;
//        }
//    }
//}

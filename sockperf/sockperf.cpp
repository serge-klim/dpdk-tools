#include "sockperf.hpp"

//void sockperf::processor::operator()(boost::asio::mutable_buffer buffer)
//{
//   assert(buffer.size() == header_size());
//   auto n = seqn(buffer.data(), buffer.size());
//   auto type = message_type(buffer.data(), buffer.size());
//   if (type == sockperf::type::Ping) {
//   //if ((static_cast<std::uint16_t>(type) & static_cast<std::uint16_t>(sockperf::type::WarmupMessage)) != 0) {
//      //message_size(buffer.data(), buffer.size());
//      message_type(sockperf::type::Pong, buffer.data(), buffer.size());
//   }
//}

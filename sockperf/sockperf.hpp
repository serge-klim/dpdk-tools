#pragma once
#include <boost/endian.hpp>
#include <cstdint>
#include <cstring>
#include <cassert>

//struct message
//{   
//   enum class Flags : std::uint16_t
//   {
//      Client = 1,
//      Pong = 2,
//      Ping =3 ,
//      WarmupMessage = 4
//   };
//   std::uint64_t seqn;
//   Flags/*std::uint16_t*/ flags;
//   std::uint32_t len;
//   char payload[2];
//};

// payload example:		
//						   flags
// [    segn - 7         ][ping] [len-14/0xe]
//00 00 00 00 00 00 00 07  00 03  00 00 00 0e

// [    segn - 7 - same  ][pong] [len-14/0xe]
//00 00 00 00 00 00 00 07  00 02  00 00 00 0e

namespace sockperf {

enum class type : std::uint16_t
{
   Client = 1,
   Pong = 2,
   Ping = 3,
   WarmupMessage = 4,
   WarmupPong = 4 | 2,
   WarmupPing = 4 | 3
};

constexpr char const* to_string(type t) noexcept {
	switch (t) {
	   case type::Pong: 
		   return "pong";
	   case type::Ping:
		   return "ping";
	   case type::WarmupPong:
		   return "warmup | pong";
	   case type::WarmupPing:
		   return "warmup | ping";
	   case type::Client:
	   case type::WarmupMessage:
			   break;
	}
	return "unknown";
}

using seqn_t  = std::uint64_t;
using message_size_t  = std::uint32_t;

constexpr/*consteval*/ auto header_size() noexcept { return sizeof(seqn_t) + sizeof(type) + sizeof(message_size_t); }

[[nodiscard]] inline /*constexpr*/ seqn_t seqn(void const* packet, [[maybe_unused]] std::size_t size) noexcept
{
   seqn_t res;
   assert(sizeof(res) <= size);
   std::memcpy(&res, packet, sizeof(res));
   return boost::endian::big_to_native(res);
}

inline void seqn(seqn_t value, void* packet, [[maybe_unused]] std::size_t size) noexcept
{
   assert(sizeof(value) <= size);
   boost::endian::native_to_big_inplace(value);
   std::memcpy(packet, &value, sizeof(value));
   assert(seqn(packet, size) == boost::endian::big_to_native(value));
}

//[[nodiscard]] inline /*constexpr*/ seqn_t increment_seqn(void const* packet, std::size_t size) noexcept
//{
//   seqn_t res;
//   assert(sizeof(res) <= size);
//   std::memcpy(&res, packet, sizeof(res));
//   return boost::endian::big_to_native(res);
//}

[[nodiscard]] inline /*constexpr*/ type message_type(void const* packet, [[maybe_unused]] std::size_t size) noexcept
{
   type res;
   assert(sizeof(seqn_t) + sizeof(res) <= size);
   std::memcpy(&res, static_cast<seqn_t const*>(packet) + 1, sizeof(res));
   return boost::endian::big_to_native(res);
}

inline void message_type(type value, void* packet, [[maybe_unused]] std::size_t size) noexcept
{
   assert(sizeof(seqn_t) + sizeof(type) <= size);
   boost::endian::native_to_big_inplace(value);
   std::memcpy(static_cast<seqn_t*>(packet) + 1, &value, sizeof(value));
   assert(message_type(packet, size) == boost::endian::big_to_native(value));
}

[[nodiscard]] inline /*constexpr*/ message_size_t message_size(void const* packet, [[maybe_unused]] std::size_t size) noexcept
{
   message_size_t res;
   assert(sizeof(seqn_t) + sizeof(type) + sizeof(res) <= size);
   std::memcpy(&res, static_cast<char const*>(packet) + sizeof(seqn_t) + sizeof(type), sizeof(res));
   return boost::endian::big_to_native(res);
}

inline void message_size(message_size_t value, void* packet, [[maybe_unused]] std::size_t size) noexcept
{
   boost::endian::native_to_big_inplace(value);
   assert(sizeof(seqn_t) + sizeof(type) + sizeof(value) <= size);
   std::memcpy(static_cast<char*>(packet) + sizeof(seqn_t) + sizeof(type), &value, sizeof(value));
   assert(message_size(packet, size) == boost::endian::big_to_native(value));
}

[[nodiscard]] inline constexpr void* payload(void* packet, [[maybe_unused]] std::size_t size) noexcept
{
   assert(header_size() <= size);
   return static_cast<char*>(packet) + header_size();
}

[[nodiscard]] inline constexpr void const* payload(void const* packet, [[maybe_unused]] std::size_t size) noexcept
{
   assert(header_size() <= size);
   return static_cast<char const*>(packet) + header_size();
}

[[nodiscard]] inline constexpr std::size_t payload_size(void* /*packet*/, std::size_t size) noexcept
{
	assert(header_size() <= size);
	return size  - header_size();
}


} // namespace sockperf

#pragma once
#include "sockperf.hpp"
#include "boost/endian/arithmetic.hpp"
#include <cstdint>

namespace sockperf::x {

static constexpr auto warmup_mask = seqn_t{ 1ULL << 63 };

constexpr auto is_warmup(seqn_t seqn) noexcept { return (seqn & warmup_mask) != 0; }
constexpr seqn_t warmup(seqn_t seqn) noexcept { return (seqn | warmup_mask); }

struct timestamps {
	boost::endian::big_uint64_t originate; //the time the sender last touched the message before sending it
	boost::endian::big_uint64_t receive;   //the time the echoer first touched it on receipt
	boost::endian::big_uint64_t transmit;  //the time the echoer last touched the message on sending it
};

static_assert(sizeof(timestamps) == sizeof(timestamps::originate) + sizeof(timestamps::receive) + sizeof(timestamps::transmit));

struct packet_info{
	seqn_t sent_seqn;
	seqn_t received_seqn;
	std::uint64_t received;
	timestamps tss;
};

constexpr seqn_t make_seqn(std::uint8_t queue, seqn_t seqn) noexcept { return (static_cast<seqn_t>(queue) << 56) | seqn; }
constexpr std::pair<std::uint8_t,seqn_t> split_seqn(seqn_t seqn) noexcept { return std::make_pair(static_cast<seqn_t>(seqn) >> 56,  seqn&0xfffffff); }

} // namespace sockperf::x


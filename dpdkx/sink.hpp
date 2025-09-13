#pragma once
#include <cstdint>

struct rte_mbuf;
struct rte_ring;

namespace dpdkx {

class device;

class sink{
public: 
    sink(dpdkx::device& device) : device_{ device } {}
    constexpr dpdkx::device const& device() const { return device_; }
    unsigned int enque(rte_mbuf** buffers, std::uint16_t n) {
        return (this->*ptr_)(buffers, n);
    }
    bool enque(struct rte_mbuf* buffer) noexcept {
        return enque(&buffer, 1) != 0;
    }
    //constexpr bool inderect() const noexcept { return ptr_ != &sink::tx_burst; }
private:
    unsigned int setup_sink(struct rte_mbuf** buffers, std::uint16_t n);
    unsigned int ring_enque(struct rte_mbuf** buffers, std::uint16_t n);
    unsigned int tx_burst(struct rte_mbuf** buffers, std::uint16_t n);
private:
    dpdkx::device& device_;
    union {
        rte_ring* ring;
        std::uint16_t queue_id;
    } id_;
    unsigned int(sink::*ptr_)(struct rte_mbuf** buffers, std::uint16_t n) = &sink::setup_sink;
};
    

} // namespace dpdkx





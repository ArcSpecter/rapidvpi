#ifndef VIP_UART_COMMON_UART_PARAMS_HPP
#define VIP_UART_COMMON_UART_PARAMS_HPP

#include <cstdint>

namespace vip::uart {

enum class UartParity : unsigned {
    NONE = 0,
    EVEN = 1,
    ODD = 2,
};

struct UartParams {
    unsigned data_bits = 8;
    unsigned stop_bits = 1;
    UartParity parity = UartParity::NONE;

    // Number of parent testbench clock rising edges per UART bit.
    unsigned bit_clks = 16;

    // Start-bit center sample offset after the start edge is detected.
    unsigned sample_clk_index = 8;

    bool idle_high = true;
    bool lsb_first = true;

    // Default physical flow-control polarity. Individual port maps can
    // override this where a wrapper uses different polarities.
    bool cts_active_low = true;
    bool rts_active_low = true;

    // Agent idle polling clock edges while no frame is active.
    unsigned idle_poll_clks = 1;

    [[nodiscard]] bool valid() const {
        return data_bits >= 5u
            && data_bits <= 8u
            && stop_bits >= 1u
            && stop_bits <= 2u
            && bit_clks > 0u
            && sample_clk_index > 0u
            && sample_clk_index <= bit_clks;
    }

    [[nodiscard]] bool parity_enable() const {
        return parity != UartParity::NONE;
    }

    [[nodiscard]] unsigned bits_per_frame() const {
        return 1u + data_bits + (parity_enable() ? 1u : 0u) + stop_bits;
    }

    [[nodiscard]] unsigned frame_clks() const {
        return bits_per_frame() * bit_clks;
    }

    [[nodiscard]] std::uint8_t data_mask() const {
        if (data_bits >= 8u) {
            return 0xffu;
        }
        return static_cast<std::uint8_t>((1u << data_bits) - 1u);
    }
};

[[nodiscard]] inline bool active_to_physical(const bool active, const bool active_low) {
    return active_low ? !active : active;
}

[[nodiscard]] inline bool physical_to_active(const bool physical, const bool active_low) {
    return active_low ? !physical : physical;
}

[[nodiscard]] inline bool uart_data_parity(std::uint8_t data, const unsigned data_bits) {
    bool parity = false;
    const unsigned n = data_bits > 8u ? 8u : data_bits;
    for (unsigned i = 0u; i < n; ++i) {
        parity = parity ^ (((data >> i) & 1u) != 0u);
    }
    return parity;
}

[[nodiscard]] inline bool uart_parity_bit(const std::uint8_t data, const UartParams& params) {
    const bool odd_ones = uart_data_parity(data, params.data_bits);
    switch (params.parity) {
        case UartParity::EVEN:
            return odd_ones;
        case UartParity::ODD:
            return !odd_ones;
        case UartParity::NONE:
        default:
            return false;
    }
}

} // namespace vip::uart

#endif // VIP_UART_COMMON_UART_PARAMS_HPP

// MIT License

// Copyright (c) 2026 Rovshan Rustamov

// Permission is hereby granted, free of charge, to any person obtaining a copy
// of this software and associated documentation files (the "Software"), to deal
// in the Software without restriction, including without limitation the rights
// to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
// copies of the Software, and to permit persons to whom the Software is
// furnished to do so, subject to the following conditions:

// The above copyright notice and this permission notice shall be included in all
// copies or substantial portions of the Software.

// THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
// IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
// FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
// AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
// LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
// OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE
// SOFTWARE.

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

    // Number of testbench clock rising edges per UART bit.
    unsigned bit_ticks = 16;

    // Start-bit center sample offset after the start edge is detected.
    unsigned sample_tick = 8;

    bool idle_high = true;
    bool lsb_first = true;

    // Default physical flow-control polarity. Individual port maps can
    // override this where a wrapper uses different polarities.
    bool cts_active_low = true;
    bool rts_active_low = true;

    // Agent idle polling while no frame is active.
    unsigned idle_poll_ticks = 1;

    [[nodiscard]] bool valid() const {
        return data_bits >= 5u
            && data_bits <= 8u
            && stop_bits >= 1u
            && stop_bits <= 2u
            && bit_ticks > 0u
            && sample_tick > 0u
            && sample_tick <= bit_ticks;
    }

    [[nodiscard]] bool parity_enable() const {
        return parity != UartParity::NONE;
    }

    [[nodiscard]] unsigned bits_per_frame() const {
        return 1u + data_bits + (parity_enable() ? 1u : 0u) + stop_bits;
    }

    [[nodiscard]] unsigned frame_ticks() const {
        return bits_per_frame() * bit_ticks;
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

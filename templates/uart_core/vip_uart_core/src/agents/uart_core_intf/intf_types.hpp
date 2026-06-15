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

#ifndef VIP_UART_CORE_AGENTS_UART_CORE_INTF_INTF_TYPES_HPP
#define VIP_UART_CORE_AGENTS_UART_CORE_INTF_INTF_TYPES_HPP

#include <cstdint>

#include "pindefs.hpp"

namespace test {

struct UartCoreConfig {
    bool enable = true;
    bool rx_enable = true;
    bool tx_enable = true;
    std::uint32_t baud_inc = BASIC_BAUD_INC;
    unsigned parity_mode = UART_PARITY_NONE;
    unsigned stop_bits = UART_STOP_1;
    unsigned data_bits = UART_DATA_8;
    bool hw_flow_enable = false;
};

struct UartCoreRxByte {
    bool valid = false;
    std::uint8_t data = 0;
    bool frame_error = false;
    bool parity_error = false;
    bool break_detect = false;
    double time_ns = -1.0;
};

struct UartCoreStatus {
    bool tx_byte_ready = false;
    bool rx_byte_valid = false;
    unsigned rx_level = 0;
    unsigned tx_level = 0;
    bool rx_empty = true;
    bool rx_full = false;
    bool tx_empty = true;
    bool tx_full = false;
    bool rx_busy = false;
    bool tx_busy = false;
    bool cts_active = true;
    bool rts_active = false;
    bool cts_blocked = false;
    bool event_rx_overrun = false;
    bool event_rx_frame_error = false;
    bool event_rx_parity_error = false;
    bool event_rx_break_detect = false;
    bool event_tx_done = false;
};

struct UartCoreEventCounts {
    std::uint64_t rx_overrun = 0;
    std::uint64_t rx_frame_error = 0;
    std::uint64_t rx_parity_error = 0;
    std::uint64_t rx_break_detect = 0;
    std::uint64_t tx_done = 0;
};

[[nodiscard]] inline UartCoreConfig make_basic_uart_core_config() {
    UartCoreConfig cfg{};
    cfg.enable = true;
    cfg.rx_enable = true;
    cfg.tx_enable = true;
    cfg.baud_inc = BASIC_BAUD_INC;
    cfg.parity_mode = UART_PARITY_NONE;
    cfg.stop_bits = UART_STOP_1;
    cfg.data_bits = UART_DATA_8;
    cfg.hw_flow_enable = false;
    return cfg;
}

[[nodiscard]] inline UartCoreRxByte make_expected_rx_byte(const std::uint8_t data) {
    UartCoreRxByte rec{};
    rec.valid = true;
    rec.data = data;
    return rec;
}

} // namespace test

#endif // VIP_UART_CORE_AGENTS_UART_CORE_INTF_INTF_TYPES_HPP

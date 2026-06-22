/*
 * MIT License
 *
 * Copyright (c) 2026 Rovshan Rustamov
 *
 * Permission is hereby granted, free of charge, to any person obtaining a copy
 * of this software and associated documentation files (the "Software"), to deal
 * in the Software without restriction, including without limitation the rights
 * to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
 * copies of the Software, and to permit persons to whom the Software is
 * furnished to do so, subject to the following conditions:
 *
 * The above copyright notice and this permission notice shall be included in all
 * copies or substantial portions of the Software.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
 * IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
 * AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
 * LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
 * OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE
 * SOFTWARE.
 */

#ifndef VIP_UART_COMMON_UART_TYPES_HPP
#define VIP_UART_COMMON_UART_TYPES_HPP

#include <cstdint>
#include <string>

#include "vip_common/common/logger.hpp"
#include "vip_uart/common/uart_params.hpp"

namespace vip::uart {

struct UartFrame {
    std::uint8_t data = 0;
    bool parity_error = false;
    bool framing_error = false;
    bool break_detect = false;
    test::sim_tick_t start_tick = vip::common::INVALID_TICK;
    test::sim_tick_t end_tick = vip::common::INVALID_TICK;

    unsigned data_bits = 8;
    unsigned stop_bits = 1;
    UartParity parity = UartParity::NONE;
};

struct UartTxPortConfig {
    // Logical name used by testcases and scoreboards.
    std::string name;

    // Physical net driven by this UART TX source.
    // In a DUT UART-core RX test this is normally the DUT uart_rx_i net.
    std::string tx_net;

    // Optional physical RTS net sampled before each frame launch.
    // In a DUT UART-core RX test this is normally the DUT uart_rts_o net.
    std::string rts_net;
    bool use_rts = false;
    bool rts_active_low = true;

    [[nodiscard]] static UartTxPortConfig from_base(const std::string& base) {
        UartTxPortConfig cfg;
        cfg.name = base;
        cfg.tx_net = base + "_tx";
        cfg.rts_net = base + "_rts";
        cfg.use_rts = false;
        return cfg;
    }
};

struct UartRxPortConfig {
    // Logical name used by testcases and scoreboards.
    std::string name;

    // Physical net sampled by this UART RX sink.
    // In a DUT UART-core TX test this is normally the DUT uart_tx_o net.
    std::string rx_net;

    // Optional physical CTS net driven to permit/block the peer transmitter.
    // In a DUT UART-core TX test this is normally the DUT uart_cts_i net.
    std::string cts_net;
    bool drive_cts = false;
    bool cts_active_low = true;

    [[nodiscard]] static UartRxPortConfig from_base(const std::string& base) {
        UartRxPortConfig cfg;
        cfg.name = base;
        cfg.rx_net = base + "_rx";
        cfg.cts_net = base + "_cts";
        cfg.drive_cts = false;
        return cfg;
    }
};

} // namespace vip::uart

#endif // VIP_UART_COMMON_UART_TYPES_HPP

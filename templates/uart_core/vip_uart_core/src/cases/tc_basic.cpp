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

#include "tc_basic.hpp"

#include <cstddef>
#include <cstdint>
#include <vector>

#include "tc_utils.hpp"

#include "../pindefs.hpp"

#include "vip_common/common/logger.hpp"

namespace test {

TestBase::RunUserTask tc_basic(Test& test) {

    co_await tc_local_reset(test);

    UartCoreStatus status{};
    co_await test.core_intf.sample_status(status);
    test.scb_core.check_idle_status(status, "tc_basic post-reset");

    const std::vector<std::uint8_t> rx_serial_bytes = {0x55u, 0xa3u};
    for (const std::uint8_t byte : rx_serial_bytes) {
        test.scb_core.expect_rx_byte(make_expected_rx_byte(byte));

        const unsigned ticket = test.uart_peer_tx.enqueue_byte(uart_rx_port_name, byte);
        co_await test.uart_peer_tx.wait_done(ticket);

        UartCoreRxByte observed{};
        co_await test.core_intf.pop_rx_byte(observed);
    }

    const std::vector<std::uint8_t> tx_byte_side_bytes = {0x3cu, 0xc5u};
    test.uart_peer_rx.clear_history(uart_tx_port_name);
    test.scb_core.expect_tx_bytes(tx_byte_side_bytes);
    test.scb_uart_stream.expect_bytes(uart_tx_port_name, tx_byte_side_bytes);

    for (const std::uint8_t byte : tx_byte_side_bytes) {
        co_await test.core_intf.push_tx_byte(byte);
    }

    co_await test.uart_peer_rx.wait_for_frames(uart_tx_port_name, tx_byte_side_bytes.size());

    const auto tx_frames = test.uart_peer_rx.get_history(uart_tx_port_name);
    for (std::size_t i = 0u; i < tx_byte_side_bytes.size() && i < tx_frames.size(); ++i) {
        test.scb_core.observe_uart_tx_frame(tx_frames.at(i));
    }

    co_await test.core_intf.wait_tx_idle();
    co_await test.core_intf.sample_status(status);
    test.scb_core.check_idle_status(status, "tc_basic final");

    if (!test.scb.case_has_failures()) {
        test.scb.note_pass("tc_basic UART RX and TX byte paths completed");
    }

    co_return;
}

} // namespace test

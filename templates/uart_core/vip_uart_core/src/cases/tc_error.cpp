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

#include "tc_error.hpp"

#include <cstddef>
#include <cstdint>
#include <string>
#include <vector>

#include "tc_utils.hpp"

#include "../pindefs.hpp"

#include "vip_common/common/logger.hpp"

namespace test {
namespace {

static constexpr unsigned ERROR_STATUS_TIMEOUT_CYCLES = BASIC_UART_BIT_CLKS * 64u;

[[nodiscard]] vip::uart::UartParity parity_from_cfg(const unsigned mode) {
    switch (mode) {
        case UART_PARITY_EVEN:
            return vip::uart::UartParity::EVEN;
        case UART_PARITY_ODD:
            return vip::uart::UartParity::ODD;
        case UART_PARITY_NONE:
        default:
            return vip::uart::UartParity::NONE;
    }
}

void log_subcase(const std::string& name) {
    vip::common::log_line("tc_error", "INFO", "subcase " + name);
}

void check_true(Test& test, const bool condition, const std::string& msg) {
    if (!condition) {
        test.scb.note_fail("tc_error: " + msg);
    }
}

TestBase::RunUserTask wait_cycles(Test& test, const unsigned cycles) {
    if (cycles != 0u) {
        co_await test.utils.clock(static_cast<int>(cycles), 1);
    }
    co_return;
}

TestBase::RunUserTask apply_error_config(Test& test,
                                         const unsigned parity_mode = UART_PARITY_NONE,
                                         const unsigned stop_mode = UART_STOP_1,
                                         const unsigned data_mode = UART_DATA_8) {
    const vip::uart::UartParams params =
        tc_uart_params_for(BASIC_BAUD_RATE, 8u, stop_mode == UART_STOP_2 ? 2u : 1u, parity_from_cfg(parity_mode));
    const UartCoreConfig cfg =
        tc_make_uart_config(BASIC_BAUD_RATE,
                            parity_mode,
                            stop_mode,
                            data_mode,
                            true,
                            true,
                            true,
                            false);

    co_await tc_apply_uart_config(test, cfg, params);
    co_await test.utils.clock(2, 1);
    co_return;
}

[[nodiscard]] vip::uart::UartParams params_for_error_config(const unsigned parity_mode = UART_PARITY_NONE,
                                                            const unsigned stop_mode = UART_STOP_1) {
    return tc_uart_params_for(BASIC_BAUD_RATE,
                              8u,
                              stop_mode == UART_STOP_2 ? 2u : 1u,
                              parity_from_cfg(parity_mode));
}

TestBase::RunUserTask send_rx_byte(Test& test,
                                   const std::uint8_t data,
                                   const vip::uart::UartParams& params) {
    const unsigned ticket = test.uart_peer_tx.enqueue_byte(uart_rx_port_name, data);
    co_await test.uart_peer_tx.wait_done(ticket);
    co_await wait_cycles(test, params.bit_clks * 4u);
    co_return;
}

TestBase::RunUserTask expect_event_delta(Test& test,
                                         const UartCoreEventCounts& before,
                                         const std::uint64_t rx_overrun_delta,
                                         const std::uint64_t rx_frame_error_delta,
                                         const std::uint64_t rx_parity_error_delta,
                                         const std::uint64_t rx_break_detect_delta,
                                         std::string label,
                                         const unsigned timeout_cycles = HANDSHAKE_TIMEOUT_CYCLES) {
    UartCoreEventCounts target{};
    target.rx_overrun = before.rx_overrun + rx_overrun_delta;
    target.rx_frame_error = before.rx_frame_error + rx_frame_error_delta;
    target.rx_parity_error = before.rx_parity_error + rx_parity_error_delta;
    target.rx_break_detect = before.rx_break_detect + rx_break_detect_delta;

    UartCoreEventCounts after = test.core_intf.event_counts();
    for (unsigned cycle = 0u; cycle < timeout_cycles; ++cycle) {
        const bool match = after.rx_overrun == target.rx_overrun
            && after.rx_frame_error == target.rx_frame_error
            && after.rx_parity_error == target.rx_parity_error
            && after.rx_break_detect == target.rx_break_detect;
        if (match) {
            co_return;
        }

        const bool overshot = after.rx_overrun > target.rx_overrun
            || after.rx_frame_error > target.rx_frame_error
            || after.rx_parity_error > target.rx_parity_error
            || after.rx_break_detect > target.rx_break_detect;
        if (overshot) {
            break;
        }

        co_await test.utils.clock(1, 1);
        after = test.core_intf.event_counts();
    }

    after = test.core_intf.event_counts();
    check_true(test, after.rx_overrun == target.rx_overrun, label + ": event_rx_overrun count mismatch");
    check_true(test, after.rx_frame_error == target.rx_frame_error, label + ": event_rx_frame_error count mismatch");
    check_true(test,
               after.rx_parity_error == target.rx_parity_error,
               label + ": event_rx_parity_error count mismatch");
    check_true(test,
               after.rx_break_detect == target.rx_break_detect,
               label + ": event_rx_break_detect count mismatch");
    co_return;
}

TestBase::RunUserTask expect_rx_record(Test& test,
                                       const std::uint8_t data,
                                       const bool frame_error,
                                       const bool parity_error,
                                       const bool break_detect,
                                       std::string label) {
    UartCoreRxByte exp = make_expected_rx_byte(data);
    exp.frame_error = frame_error;
    exp.parity_error = parity_error;
    exp.break_detect = break_detect;
    test.scb_core.expect_rx_byte(exp);

    UartCoreRxByte observed{};
    co_await test.core_intf.pop_rx_byte(observed, HANDSHAKE_TIMEOUT_CYCLES * 4u);
    if (observed.valid) {
        check_true(test, observed.data == data, label + ": RX data mismatch");
        check_true(test, observed.frame_error == frame_error, label + ": RX frame_error mismatch");
        check_true(test, observed.parity_error == parity_error, label + ": RX parity_error mismatch");
        check_true(test, observed.break_detect == break_detect, label + ": RX break_detect mismatch");
    }
    co_return;
}

[[nodiscard]] bool rx_status_matches(const UartCoreStatus& status,
                                     const unsigned level,
                                     const bool empty,
                                     const bool full) {
    return status.rx_level == level
        && status.rx_empty == empty
        && status.rx_full == full
        && status.rx_byte_valid == !empty;
}

void expect_rx_fifo_status(Test& test,
                           const UartCoreStatus& status,
                           const unsigned level,
                           const bool empty,
                           const bool full,
                           const std::string& label) {
    check_true(test, status.rx_level == level, label + ": rx_fifo_level mismatch");
    check_true(test, status.rx_empty == empty, label + ": rx_fifo_empty mismatch");
    check_true(test, status.rx_full == full, label + ": rx_fifo_full mismatch");
    check_true(test,
               status.rx_byte_valid == !empty,
               label + ": rx_byte_valid did not follow RX FIFO non-empty state");
}

TestBase::RunUserTask wait_rx_fifo_status(Test& test,
                                          const unsigned level,
                                          const bool empty,
                                          const bool full,
                                          std::string label,
                                          const unsigned timeout_cycles =
                                              ERROR_STATUS_TIMEOUT_CYCLES) {
    UartCoreStatus status{};
    for (unsigned cycle = 0u; cycle < timeout_cycles; ++cycle) {
        co_await test.core_intf.sample_status(status);
        if (rx_status_matches(status, level, empty, full)) {
            co_return;
        }
        co_await test.utils.clock(1, 1);
    }

    test.scb.note_fail("tc_error: " + label + ": RX FIFO status wait timed out");
    expect_rx_fifo_status(test, status, level, empty, full, label);
    co_return;
}

[[nodiscard]] std::vector<std::uint8_t> make_bytes(const std::size_t count,
                                                   const std::uint8_t first) {
    std::vector<std::uint8_t> data;
    data.reserve(count);
    for (std::size_t i = 0u; i < count; ++i) {
        data.push_back(static_cast<std::uint8_t>(first + i));
    }
    return data;
}

TestBase::RunUserTask subcase_clean_baseline(Test& test) {
    log_subcase("clean baseline no-error frame");
    co_await tc_local_reset(test);
    co_await apply_error_config(test);

    const vip::uart::UartParams params = params_for_error_config();
    const UartCoreEventCounts before = test.core_intf.event_counts();

    co_await send_rx_byte(test, 0x55u, params);
    co_await expect_rx_record(test, 0x55u, false, false, false, "clean baseline");
    co_await expect_event_delta(test, before, 0u, 0u, 0u, 0u, "clean baseline");
    co_return;
}

TestBase::RunUserTask subcase_even_parity_error(Test& test) {
    log_subcase("even parity error");
    co_await tc_local_reset(test);
    co_await apply_error_config(test, UART_PARITY_EVEN);

    const vip::uart::UartParams params = params_for_error_config(UART_PARITY_EVEN);

    const UartCoreEventCounts before_good = test.core_intf.event_counts();
    co_await send_rx_byte(test, 0x3cu, params);
    co_await expect_rx_record(test, 0x3cu, false, false, false, "even parity good frame");
    co_await expect_event_delta(test, before_good, 0u, 0u, 0u, 0u, "even parity good frame");

    const UartCoreEventCounts before_bad = test.core_intf.event_counts();
    test.uart_peer_tx.arm_next_parity_error(uart_rx_port_name);
    co_await send_rx_byte(test, 0x35u, params);
    co_await expect_rx_record(test, 0x35u, false, true, false, "even parity bad frame");
    co_await expect_event_delta(test, before_bad, 0u, 0u, 1u, 0u, "even parity bad frame");
    co_return;
}

TestBase::RunUserTask subcase_odd_parity_error(Test& test) {
    log_subcase("odd parity error");
    co_await tc_local_reset(test);
    co_await apply_error_config(test, UART_PARITY_ODD);

    const vip::uart::UartParams params = params_for_error_config(UART_PARITY_ODD);

    const UartCoreEventCounts before_good = test.core_intf.event_counts();
    co_await send_rx_byte(test, 0x5au, params);
    co_await expect_rx_record(test, 0x5au, false, false, false, "odd parity good frame");
    co_await expect_event_delta(test, before_good, 0u, 0u, 0u, 0u, "odd parity good frame");

    const UartCoreEventCounts before_bad = test.core_intf.event_counts();
    test.uart_peer_tx.arm_next_parity_error(uart_rx_port_name);
    co_await send_rx_byte(test, 0xa6u, params);
    co_await expect_rx_record(test, 0xa6u, false, true, false, "odd parity bad frame");
    co_await expect_event_delta(test, before_bad, 0u, 0u, 1u, 0u, "odd parity bad frame");
    co_return;
}

TestBase::RunUserTask subcase_frame_error(Test& test) {
    log_subcase("frame error on stop bit");
    co_await tc_local_reset(test);
    co_await apply_error_config(test);

    const vip::uart::UartParams params = params_for_error_config();
    const UartCoreEventCounts before = test.core_intf.event_counts();

    test.uart_peer_tx.arm_next_framing_error(uart_rx_port_name);
    co_await send_rx_byte(test, 0xc3u, params);
    co_await expect_rx_record(test, 0xc3u, true, false, false, "bad stop bit");
    co_await expect_event_delta(test, before, 0u, 1u, 0u, 0u, "bad stop bit");
    co_return;
}

TestBase::RunUserTask subcase_break_detect(Test& test) {
    log_subcase("break detect");
    co_await tc_local_reset(test);
    co_await apply_error_config(test);

    const vip::uart::UartParams params = params_for_error_config();
    const UartCoreEventCounts before_break = test.core_intf.event_counts();

    test.uart_peer_tx.arm_next_framing_error(uart_rx_port_name);
    co_await send_rx_byte(test, 0x00u, params);
    co_await expect_rx_record(test, 0x00u, true, false, true, "break-like frame");
    co_await expect_event_delta(test, before_break, 0u, 1u, 0u, 1u, "break-like frame");

    co_await wait_cycles(test, params.frame_clks() + (params.bit_clks * 2u));

    const UartCoreEventCounts before_recovery = test.core_intf.event_counts();
    co_await send_rx_byte(test, 0x5au, params);
    co_await expect_rx_record(test, 0x5au, false, false, false, "break recovery clean frame");
    co_await expect_event_delta(test, before_recovery, 0u, 0u, 0u, 0u, "break recovery clean frame");
    co_return;
}

TestBase::RunUserTask subcase_rx_overrun(Test& test) {
    log_subcase("RX overrun");
    co_await tc_local_reset(test);
    co_await apply_error_config(test);

    const vip::uart::UartParams params = params_for_error_config();
    const std::vector<std::uint8_t> data = make_bytes(RX_FIFO_DEPTH, 0x80u);

    co_await test.core_intf.set_rx_ready(false);
    for (const std::uint8_t byte : data) {
        co_await send_rx_byte(test, byte, params);
    }
    co_await wait_rx_fifo_status(test, RX_FIFO_DEPTH, false, true, "RX overrun prefill full");

    const UartCoreEventCounts before = test.core_intf.event_counts();
    co_await send_rx_byte(test, 0xe1u, params);
    co_await expect_event_delta(test, before, 1u, 0u, 0u, 0u, "RX overrun extra frame");
    co_await wait_rx_fifo_status(test, RX_FIFO_DEPTH, false, true, "RX overrun stayed full");

    for (std::size_t i = 0u; i < data.size(); ++i) {
        co_await expect_rx_record(test,
                                  data.at(i),
                                  false,
                                  false,
                                  false,
                                  "RX overrun ordered pop " + std::to_string(i));
    }

    co_await wait_rx_fifo_status(test, 0u, true, false, "RX overrun final empty");
    co_return;
}

TestBase::RunUserTask subcase_error_metadata_ordering(Test& test) {
    log_subcase("error metadata ordering through RX FIFO");
    co_await tc_local_reset(test);
    co_await apply_error_config(test, UART_PARITY_EVEN);

    const vip::uart::UartParams params = params_for_error_config(UART_PARITY_EVEN);
    const UartCoreEventCounts before = test.core_intf.event_counts();

    co_await test.core_intf.set_rx_ready(false);
    co_await send_rx_byte(test, 0x11u, params);
    test.uart_peer_tx.arm_next_parity_error(uart_rx_port_name);
    co_await send_rx_byte(test, 0x22u, params);
    co_await send_rx_byte(test, 0x33u, params);

    co_await wait_rx_fifo_status(test, 3u, false, false, "metadata ordering pre-pop");
    co_await expect_event_delta(test, before, 0u, 0u, 1u, 0u, "metadata ordering parity event");

    co_await expect_rx_record(test, 0x11u, false, false, false, "metadata ordering A");
    co_await expect_rx_record(test, 0x22u, false, true, false, "metadata ordering B");
    co_await expect_rx_record(test, 0x33u, false, false, false, "metadata ordering C");
    co_await wait_rx_fifo_status(test, 0u, true, false, "metadata ordering final empty");
    co_return;
}

} // namespace

TestBase::RunUserTask tc_error(Test& test) {

    co_await subcase_clean_baseline(test);
    co_await subcase_even_parity_error(test);
    co_await subcase_odd_parity_error(test);
    co_await subcase_frame_error(test);
    co_await subcase_break_detect(test);
    co_await subcase_rx_overrun(test);
    co_await subcase_error_metadata_ordering(test);

    if (!test.scb.case_has_failures()) {
        test.scb.note_pass("tc_error RX error behavior completed");
    }

    co_return;
}

} // namespace test

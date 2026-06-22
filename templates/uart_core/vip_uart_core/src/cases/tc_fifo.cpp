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

#include "tc_fifo.hpp"

#include <algorithm>
#include <cstddef>
#include <cstdint>
#include <string>
#include <vector>

#include "tc_utils.hpp"

#include "../pindefs.hpp"

#include "vip_common/common/logger.hpp"

namespace test {
namespace {

static constexpr unsigned FIFO_STATUS_TIMEOUT_CYCLES = BASIC_UART_BIT_CLKS * 32u;
static constexpr unsigned SHORT_READY_TIMEOUT_CYCLES = 8u;

void log_subcase(const std::string& name) {
    vip::common::log_line("tc_fifo", "INFO", "subcase " + name);
}

void check_true(Test& test, const bool condition, const std::string& msg) {
    if (!condition) {
        test.scb.note_fail("tc_fifo: " + msg);
    }
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

[[nodiscard]] unsigned tx_timeout_for(const vip::uart::UartParams& params,
                                      const std::size_t frames) {
    return (params.frame_clks() * static_cast<unsigned>(frames + 2u))
        + (HANDSHAKE_TIMEOUT_CYCLES * 4u);
}

TestBase::RunUserTask wait_cycles(Test& test, const unsigned cycles) {
    if (cycles != 0u) {
        co_await test.utils.clock(static_cast<int>(cycles), 1);
    }
    co_return;
}

TestBase::RunUserTask apply_fifo_config(Test& test,
                                        const bool rx_enable = true,
                                        const bool tx_enable = true) {
    const UartCoreConfig cfg =
        tc_make_uart_config(BASIC_BAUD_RATE,
                            UART_PARITY_NONE,
                            UART_STOP_1,
                            UART_DATA_8,
                            true,
                            rx_enable,
                            tx_enable,
                            false);
    co_await tc_apply_uart_config(test, cfg, make_uart_params());
    co_await test.utils.clock(2, 1);
    co_return;
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

void expect_tx_fifo_status(Test& test,
                           const UartCoreStatus& status,
                           const unsigned level,
                           const bool empty,
                           const bool full,
                           const std::string& label) {
    check_true(test, status.tx_level == level, label + ": tx_fifo_level mismatch");
    check_true(test, status.tx_empty == empty, label + ": tx_fifo_empty mismatch");
    check_true(test, status.tx_full == full, label + ": tx_fifo_full mismatch");
    check_true(test,
               status.tx_byte_ready == !full,
               label + ": tx_byte_ready did not follow TX FIFO space");
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

[[nodiscard]] bool tx_status_matches(const UartCoreStatus& status,
                                     const unsigned level,
                                     const bool empty,
                                     const bool full) {
    return status.tx_level == level
        && status.tx_empty == empty
        && status.tx_full == full
        && status.tx_byte_ready == !full;
}

TestBase::RunUserTask wait_rx_fifo_status(Test& test,
                                          const unsigned level,
                                          const bool empty,
                                          const bool full,
                                          std::string label,
                                          UartCoreStatus* final_status = nullptr,
                                          const unsigned timeout_cycles =
                                              FIFO_STATUS_TIMEOUT_CYCLES) {
    UartCoreStatus status{};
    for (unsigned cycle = 0u; cycle < timeout_cycles; ++cycle) {
        co_await test.core_intf.sample_status(status);
        if (rx_status_matches(status, level, empty, full)) {
            if (final_status != nullptr) {
                *final_status = status;
            }
            co_return;
        }
        co_await test.utils.clock(1, 1);
    }

    test.scb.note_fail("tc_fifo: " + label + ": RX FIFO status wait timed out");
    expect_rx_fifo_status(test, status, level, empty, full, label);
    if (final_status != nullptr) {
        *final_status = status;
    }
    co_return;
}

TestBase::RunUserTask wait_tx_fifo_status(Test& test,
                                          const unsigned level,
                                          const bool empty,
                                          const bool full,
                                          std::string label,
                                          UartCoreStatus* final_status = nullptr,
                                          const unsigned timeout_cycles =
                                              FIFO_STATUS_TIMEOUT_CYCLES) {
    UartCoreStatus status{};
    for (unsigned cycle = 0u; cycle < timeout_cycles; ++cycle) {
        co_await test.core_intf.sample_status(status);
        if (tx_status_matches(status, level, empty, full)) {
            if (final_status != nullptr) {
                *final_status = status;
            }
            co_return;
        }
        co_await test.utils.clock(1, 1);
    }

    test.scb.note_fail("tc_fifo: " + label + ": TX FIFO status wait timed out");
    expect_tx_fifo_status(test, status, level, empty, full, label);
    if (final_status != nullptr) {
        *final_status = status;
    }
    co_return;
}

TestBase::RunUserTask send_rx_byte_to_fifo(Test& test,
                                           const std::uint8_t data,
                                           const vip::uart::UartParams& params) {
    const unsigned ticket = test.uart_peer_tx.enqueue_byte(uart_rx_port_name, data);
    co_await test.uart_peer_tx.wait_done(ticket);
    co_await wait_cycles(test, params.bit_clks * 4u);
    co_return;
}

TestBase::RunUserTask fill_rx_fifo(Test& test,
                                   const std::vector<std::uint8_t>& data,
                                   const vip::uart::UartParams& params,
                                   const bool check_each_level) {
    co_await test.core_intf.set_rx_ready(false);
    for (std::size_t i = 0u; i < data.size(); ++i) {
        co_await send_rx_byte_to_fifo(test, data.at(i), params);
        if (check_each_level) {
            const unsigned level = static_cast<unsigned>(i + 1u);
            co_await wait_rx_fifo_status(test,
                                         level,
                                         false,
                                         level == RX_FIFO_DEPTH,
                                         "RX fill level " + std::to_string(level));
        }
    }
    co_return;
}

TestBase::RunUserTask pop_rx_expect(Test& test,
                                    const std::uint8_t data,
                                    std::string label) {
    test.scb_core.expect_rx_byte(make_expected_rx_byte(data));

    UartCoreRxByte observed{};
    co_await test.core_intf.pop_rx_byte(observed, HANDSHAKE_TIMEOUT_CYCLES * 4u);
    if (observed.valid) {
        check_true(test, observed.data == data, label + ": RX data mismatch");
        check_true(test, !observed.frame_error, label + ": unexpected frame_error");
        check_true(test, !observed.parity_error, label + ": unexpected parity_error");
        check_true(test, !observed.break_detect, label + ": unexpected break_detect");
    }
    co_return;
}

TestBase::RunUserTask fill_tx_fifo_disabled(Test& test,
                                            const std::vector<std::uint8_t>& data,
                                            const bool check_each_level) {
    for (std::size_t i = 0u; i < data.size(); ++i) {
        co_await test.core_intf.push_tx_byte(data.at(i));
        if (check_each_level) {
            const unsigned level = static_cast<unsigned>(i + 1u);
            co_await wait_tx_fifo_status(test,
                                         level,
                                         false,
                                         level == TX_FIFO_DEPTH,
                                         "TX fill level " + std::to_string(level));
        }
    }
    co_return;
}

TestBase::RunUserTask wait_tx_frames(Test& test,
                                     const std::size_t expected_count,
                                     const unsigned timeout_cycles,
                                     std::string label,
                                     bool& observed) {
    observed = false;
    for (unsigned cycle = 0u; cycle < timeout_cycles; ++cycle) {
        if (test.uart_peer_rx.observed_count(uart_tx_port_name) >= expected_count) {
            observed = true;
            co_return;
        }
        co_await test.utils.clock(1, 1);
    }

    test.scb.note_fail("tc_fifo: " + label + ": UART TX frame wait timed out");
    co_return;
}

TestBase::RunUserTask wait_tx_busy(Test& test,
                                   std::string label,
                                   const unsigned timeout_cycles = HANDSHAKE_TIMEOUT_CYCLES) {
    for (unsigned cycle = 0u; cycle < timeout_cycles; ++cycle) {
        UartCoreStatus status{};
        co_await test.core_intf.sample_status(status);
        if (status.tx_busy) {
            co_return;
        }
        co_await test.utils.clock(1, 1);
    }

    test.scb.note_fail("tc_fifo: " + label + ": tx_busy wait timed out");
    co_return;
}

void expect_uart_tx_bytes(Test& test, const std::vector<std::uint8_t>& data) {
    test.scb_uart_stream.expect_bytes(uart_tx_port_name, data);
    test.scb_core.expect_tx_bytes(data);
}

void observe_tx_history(Test& test,
                        const std::size_t expected_count,
                        const std::string& label) {
    const auto history = test.uart_peer_rx.get_history(uart_tx_port_name);
    if (history.size() < expected_count) {
        test.scb.note_fail("tc_fifo: " + label + ": missing UART TX frames");
    }
    if (history.size() > expected_count) {
        test.scb.note_fail("tc_fifo: " + label + ": extra UART TX frames observed");
    }

    const std::size_t n = std::min(history.size(), expected_count);
    for (std::size_t i = 0u; i < n; ++i) {
        test.scb_core.observe_uart_tx_frame(history.at(i));
    }
}

TestBase::RunUserTask verify_no_extra_tx_frames(Test& test,
                                                const std::size_t allowed_count,
                                                const unsigned cycles,
                                                std::string label) {
    for (unsigned cycle = 0u; cycle < cycles; ++cycle) {
        if (test.uart_peer_rx.observed_count(uart_tx_port_name) > allowed_count) {
            test.scb.note_fail("tc_fifo: " + label + ": unexpected UART TX frame observed");
            co_return;
        }
        co_await test.utils.clock(1, 1);
    }
    co_return;
}

TestBase::RunUserTask subcase_reset_fifo_status(Test& test) {
    log_subcase("reset FIFO status");
    co_await tc_local_reset(test);

    UartCoreStatus status{};
    co_await test.core_intf.sample_status(status);
    expect_rx_fifo_status(test, status, 0u, true, false, "reset RX");
    expect_tx_fifo_status(test, status, 0u, true, false, "reset TX");
    test.scb_core.check_idle_status(status, "tc_fifo reset");
    co_return;
}

TestBase::RunUserTask subcase_rx_fill_full(Test& test) {
    log_subcase("RX fill/full");
    co_await tc_local_reset(test);

    const vip::uart::UartParams params = make_uart_params();
    const UartCoreEventCounts before = test.core_intf.event_counts();
    const std::vector<std::uint8_t> data = make_bytes(RX_FIFO_DEPTH, 0x10u);

    co_await fill_rx_fifo(test, data, params, true);

    UartCoreStatus status{};
    co_await wait_rx_fifo_status(test,
                                 RX_FIFO_DEPTH,
                                 false,
                                 true,
                                 "RX full",
                                 &status);
    check_true(test, status.rx_byte_valid, "RX full did not assert rx_byte_valid");
    check_true(test,
               test.core_intf.event_counts().rx_overrun == before.rx_overrun,
               "RX fill to exact depth produced event_rx_overrun");
    co_return;
}

TestBase::RunUserTask subcase_rx_ordered_pop(Test& test) {
    log_subcase("RX ordered pop");
    co_await tc_local_reset(test);

    const vip::uart::UartParams params = make_uart_params();
    const std::vector<std::uint8_t> data = make_bytes(RX_FIFO_DEPTH, 0x30u);

    co_await fill_rx_fifo(test, data, params, false);
    co_await wait_rx_fifo_status(test, RX_FIFO_DEPTH, false, true, "RX pop prefill");

    for (std::size_t i = 0u; i < data.size(); ++i) {
        co_await pop_rx_expect(test, data.at(i), "RX ordered pop " + std::to_string(i));
        const unsigned level = static_cast<unsigned>(data.size() - i - 1u);
        co_await wait_rx_fifo_status(test,
                                     level,
                                     level == 0u,
                                     false,
                                     "RX pop level " + std::to_string(level));
    }
    co_return;
}

TestBase::RunUserTask subcase_rx_clear(Test& test) {
    log_subcase("RX clear");
    co_await tc_local_reset(test);

    const vip::uart::UartParams params = make_uart_params();
    const std::vector<std::uint8_t> stale = make_bytes(4u, 0x50u);

    co_await fill_rx_fifo(test, stale, params, false);
    co_await wait_rx_fifo_status(test, stale.size(), false, false, "RX clear prefill");

    co_await test.core_intf.pulse_rx_fifo_clear();
    co_await wait_rx_fifo_status(test, 0u, true, false, "RX clear emptied FIFO");

    co_await send_rx_byte_to_fifo(test, 0x8fu, params);
    co_await wait_rx_fifo_status(test, 1u, false, false, "RX clear post-refill");
    co_await pop_rx_expect(test, 0x8fu, "RX clear post-refill pop");
    co_await wait_rx_fifo_status(test, 0u, true, false, "RX clear final empty");
    co_return;
}

TestBase::RunUserTask subcase_tx_fill_full_ready(Test& test) {
    log_subcase("TX fill/full/ready");
    co_await tc_local_reset(test);
    co_await apply_fifo_config(test, true, false);

    const std::vector<std::uint8_t> data = make_bytes(TX_FIFO_DEPTH, 0x70u);
    test.uart_peer_rx.clear_history(uart_tx_port_name);

    co_await fill_tx_fifo_disabled(test, data, true);

    UartCoreStatus status{};
    co_await wait_tx_fifo_status(test,
                                 TX_FIFO_DEPTH,
                                 false,
                                 true,
                                 "TX full",
                                 &status);
    check_true(test, !status.tx_byte_ready, "TX full did not deassert tx_byte_ready");

    bool accepted = true;
    co_await test.core_intf.try_push_tx_byte(0xeeu, accepted, SHORT_READY_TIMEOUT_CYCLES);
    check_true(test, !accepted, "TX full accepted an extra byte");
    co_await wait_tx_fifo_status(test, TX_FIFO_DEPTH, false, true, "TX full after rejected push");
    co_return;
}

TestBase::RunUserTask subcase_tx_ordered_drain(Test& test) {
    log_subcase("TX ordered drain");
    co_await tc_local_reset(test);
    co_await apply_fifo_config(test, true, false);

    const vip::uart::UartParams params = make_uart_params();
    const std::vector<std::uint8_t> data = make_bytes(TX_FIFO_DEPTH, 0x90u);
    test.uart_peer_rx.clear_history(uart_tx_port_name);

    co_await fill_tx_fifo_disabled(test, data, false);
    co_await wait_tx_fifo_status(test, TX_FIFO_DEPTH, false, true, "TX drain prefill");

    const UartCoreEventCounts before = test.core_intf.event_counts();
    expect_uart_tx_bytes(test, data);
    co_await apply_fifo_config(test, true, true);

    bool observed = false;
    co_await wait_tx_frames(test, data.size(), tx_timeout_for(params, data.size()), "TX drain", observed);
    observe_tx_history(test, data.size(), "TX drain");

    co_await test.core_intf.wait_tx_idle();
    co_await wait_tx_fifo_status(test, 0u, true, false, "TX drain final empty");
    check_true(test,
               test.core_intf.event_counts().tx_done == before.tx_done + data.size(),
               "TX drain event_tx_done count mismatch");
    co_return;
}

TestBase::RunUserTask subcase_tx_clear_idle(Test& test) {
    log_subcase("TX clear idle");
    co_await tc_local_reset(test);
    co_await apply_fifo_config(test, true, false);

    const vip::uart::UartParams params = make_uart_params();
    const std::vector<std::uint8_t> data = make_bytes(4u, 0xb0u);
    test.uart_peer_rx.clear_history(uart_tx_port_name);

    co_await fill_tx_fifo_disabled(test, data, false);
    co_await wait_tx_fifo_status(test, data.size(), false, false, "TX idle clear prefill");

    const UartCoreEventCounts before = test.core_intf.event_counts();
    co_await test.core_intf.pulse_tx_fifo_clear();
    co_await wait_tx_fifo_status(test, 0u, true, false, "TX idle clear emptied FIFO");

    co_await apply_fifo_config(test, true, true);
    co_await verify_no_extra_tx_frames(test,
                                       0u,
                                       params.frame_clks() * 2u,
                                       "TX idle clear");
    check_true(test,
               test.core_intf.event_counts().tx_done == before.tx_done,
               "TX idle clear produced event_tx_done");
    co_return;
}

TestBase::RunUserTask subcase_tx_clear_active(Test& test) {
    log_subcase("TX clear active character");
    co_await tc_local_reset(test);
    co_await apply_fifo_config(test, true, false);

    const vip::uart::UartParams params = make_uart_params();
    const std::vector<std::uint8_t> data = {0x21u, 0x22u, 0x23u, 0x24u};
    test.uart_peer_rx.clear_history(uart_tx_port_name);

    co_await fill_tx_fifo_disabled(test, data, false);
    co_await wait_tx_fifo_status(test, data.size(), false, false, "TX active clear prefill");

    const std::vector<std::uint8_t> active_only = {data.front()};
    const UartCoreEventCounts before = test.core_intf.event_counts();
    expect_uart_tx_bytes(test, active_only);

    co_await apply_fifo_config(test, true, true);
    co_await wait_tx_busy(test, "TX active clear launch");
    co_await test.core_intf.pulse_tx_fifo_clear();

    bool observed = false;
    co_await wait_tx_frames(test, active_only.size(), tx_timeout_for(params, 1u), "TX active clear", observed);
    observe_tx_history(test, active_only.size(), "TX active clear");
    co_await verify_no_extra_tx_frames(test,
                                       active_only.size(),
                                       params.frame_clks() * 2u,
                                       "TX active clear");

    co_await test.core_intf.wait_tx_idle();
    co_await wait_tx_fifo_status(test, 0u, true, false, "TX active clear final empty");
    check_true(test,
               test.core_intf.event_counts().tx_done == before.tx_done + 1u,
               "TX active clear event_tx_done count mismatch");
    co_return;
}

TestBase::RunUserTask subcase_parallel_rx_fill_tx_drain(Test& test) {
    log_subcase("parallel RX fill + TX drain");
    co_await tc_local_reset(test);
    co_await apply_fifo_config(test, true, false);

    const vip::uart::UartParams params = make_uart_params();
    const std::vector<std::uint8_t> tx_data = make_bytes(6u, 0xc0u);
    const std::vector<std::uint8_t> rx_data = make_bytes(6u, 0xd0u);
    test.uart_peer_rx.clear_history(uart_tx_port_name);
    co_await test.core_intf.set_rx_ready(false);

    co_await fill_tx_fifo_disabled(test, tx_data, false);
    co_await wait_tx_fifo_status(test, tx_data.size(), false, false, "parallel TX prefill");

    expect_uart_tx_bytes(test, tx_data);
    const unsigned rx_ticket = test.uart_peer_tx.enqueue_bytes(uart_rx_port_name, rx_data);
    co_await apply_fifo_config(test, true, true);

    co_await test.uart_peer_tx.wait_done(rx_ticket);
    co_await wait_cycles(test, params.bit_clks * 4u);
    co_await wait_rx_fifo_status(test, rx_data.size(), false, false, "parallel RX fill");

    bool observed = false;
    co_await wait_tx_frames(test,
                            tx_data.size(),
                            tx_timeout_for(params, tx_data.size()),
                            "parallel TX drain",
                            observed);
    observe_tx_history(test, tx_data.size(), "parallel TX drain");

    for (std::size_t i = 0u; i < rx_data.size(); ++i) {
        co_await pop_rx_expect(test, rx_data.at(i), "parallel RX pop " + std::to_string(i));
    }

    co_await test.core_intf.wait_tx_idle();
    co_await wait_rx_fifo_status(test, 0u, true, false, "parallel final RX empty");
    co_await wait_tx_fifo_status(test, 0u, true, false, "parallel final TX empty");
    co_return;
}

TestBase::RunUserTask subcase_cross_clear_independence(Test& test) {
    log_subcase("cross-clear independence");
    co_await tc_local_reset(test);
    co_await apply_fifo_config(test, true, false);

    const vip::uart::UartParams params = make_uart_params();
    const std::vector<std::uint8_t> rx_a = {0x31u, 0x32u};
    const std::vector<std::uint8_t> tx_a = {0x41u, 0x42u};
    const std::vector<std::uint8_t> rx_b = {0x51u, 0x52u};
    const std::vector<std::uint8_t> tx_b = {0x61u, 0x62u};

    test.uart_peer_rx.clear_history(uart_tx_port_name);
    co_await test.core_intf.set_rx_ready(false);

    co_await fill_rx_fifo(test, rx_a, params, false);
    co_await fill_tx_fifo_disabled(test, tx_a, false);
    co_await wait_rx_fifo_status(test, rx_a.size(), false, false, "cross-clear pre RX clear RX");
    co_await wait_tx_fifo_status(test, tx_a.size(), false, false, "cross-clear pre RX clear TX");

    co_await test.core_intf.pulse_rx_fifo_clear();
    co_await wait_rx_fifo_status(test, 0u, true, false, "cross-clear RX cleared");

    UartCoreStatus status{};
    co_await test.core_intf.sample_status(status);
    expect_tx_fifo_status(test,
                          status,
                          static_cast<unsigned>(tx_a.size()),
                          false,
                          false,
                          "cross-clear RX did not affect TX");

    expect_uart_tx_bytes(test, tx_a);
    co_await apply_fifo_config(test, true, true);

    bool observed = false;
    co_await wait_tx_frames(test, tx_a.size(), tx_timeout_for(params, tx_a.size()), "cross-clear TX A", observed);
    observe_tx_history(test, tx_a.size(), "cross-clear TX A");
    co_await test.core_intf.wait_tx_idle();

    co_await apply_fifo_config(test, true, false);
    test.uart_peer_rx.clear_history(uart_tx_port_name);
    co_await test.core_intf.set_rx_ready(false);

    co_await fill_rx_fifo(test, rx_b, params, false);
    co_await fill_tx_fifo_disabled(test, tx_b, false);
    co_await wait_rx_fifo_status(test, rx_b.size(), false, false, "cross-clear pre TX clear RX");
    co_await wait_tx_fifo_status(test, tx_b.size(), false, false, "cross-clear pre TX clear TX");

    const UartCoreEventCounts before = test.core_intf.event_counts();
    co_await test.core_intf.pulse_tx_fifo_clear();
    co_await wait_tx_fifo_status(test, 0u, true, false, "cross-clear TX cleared");

    co_await test.core_intf.sample_status(status);
    expect_rx_fifo_status(test,
                          status,
                          static_cast<unsigned>(rx_b.size()),
                          false,
                          false,
                          "cross-clear TX did not affect RX");

    co_await apply_fifo_config(test, true, true);
    co_await verify_no_extra_tx_frames(test,
                                       0u,
                                       params.frame_clks() * 2u,
                                       "cross-clear TX clear");
    check_true(test,
               test.core_intf.event_counts().tx_done == before.tx_done,
               "cross-clear TX clear produced event_tx_done");

    for (std::size_t i = 0u; i < rx_b.size(); ++i) {
        co_await pop_rx_expect(test, rx_b.at(i), "cross-clear RX B pop " + std::to_string(i));
    }

    co_await wait_rx_fifo_status(test, 0u, true, false, "cross-clear final RX empty");
    co_await wait_tx_fifo_status(test, 0u, true, false, "cross-clear final TX empty");
    co_return;
}

} // namespace

TestBase::RunUserTask tc_fifo(Test& test) {

    co_await subcase_reset_fifo_status(test);
    co_await subcase_rx_fill_full(test);
    co_await subcase_rx_ordered_pop(test);
    co_await subcase_rx_clear(test);
    co_await subcase_tx_fill_full_ready(test);
    co_await subcase_tx_ordered_drain(test);
    co_await subcase_tx_clear_idle(test);
    co_await subcase_tx_clear_active(test);
    co_await subcase_parallel_rx_fill_tx_drain(test);
    co_await subcase_cross_clear_independence(test);

    if (!test.scb.case_has_failures()) {
        test.scb.note_pass("tc_fifo FIFO behavior completed");
    }

    co_return;
}

} // namespace test

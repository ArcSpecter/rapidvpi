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

#include "tc_reset.hpp"

// DUT parameter requirements for this testcase:
//   No tc_reset-specific DUT generic overrides are required.
// The case assumes the normal uart_core build and re-applies cfg_* inputs after
// each reset release because the core does not own those configuration values.

#include <algorithm>
#include <cstddef>
#include <cstdint>
#include <functional>
#include <string>
#include <vector>

#include "tc_utils.hpp"

#include "../pindefs.hpp"

#include "vip_common/common/logger.hpp"

namespace test {
namespace {

static constexpr unsigned RESET_LOW_CYCLES = 8u;
static constexpr unsigned RESET_POST_RELEASE_CYCLES = 2u;
static constexpr unsigned RESET_STATUS_TIMEOUT_CYCLES = BASIC_UART_BIT_CLKS * 64u;
static constexpr unsigned NO_STALE_RX_CYCLES = 12u;

struct ResetSample {
    UartCoreStatus status{};
    bool rst_pin = true;
    bool uart_tx_pin = true;
    bool uart_rx_pin = true;
};

void log_subcase(const std::string& name) {
    vip::common::log_line("tc_reset", "INFO", "subcase " + name);
}

void check_true(Test& test, const bool condition, const std::string& msg) {
    if (!condition) {
        test.scb.note_fail("tc_reset: " + msg);
    }
}

TestBase::RunUserTask wait_cycles(Test& test, const unsigned cycles) {
    if (cycles != 0u) {
        co_await test.utils.clock(static_cast<int>(cycles), 1);
    }
    co_return;
}

[[nodiscard]] unsigned tx_timeout_for(const vip::uart::UartParams& params,
                                      const std::size_t frames) {
    return (params.frame_clks() * static_cast<unsigned>(frames + 2u))
        + (HANDSHAKE_TIMEOUT_CYCLES * 4u);
}

TestBase::RunUserTask apply_reset_config(Test& test,
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
    co_await wait_cycles(test, 2u);
    co_return;
}

void clear_post_reset_scoreboard_state(Test& test) {
    test.uart_peer_rx.clear_history(uart_tx_port_name);
    test.scb_uart_stream.reset_case();
    test.scb_core.reset_case();
}

void set_tx_monitor_strict(Test& test, const bool enable) {
    test.uart_peer_rx.set_capture_enable(uart_tx_port_name, enable);
    test.scb_uart_stream.set_fail_on_unexpected(enable);
    test.scb_uart_rules.set_enable(enable);
}

TestBase::RunUserTask drive_reset_asserted(Test& test, const bool asserted) {
    co_await test.utils.clock_to_write(1, 0);

    auto w = test.getCoWrite();
    w.write(rst_n, asserted ? 0 : 1);
    co_await w;
    co_return;
}

TestBase::RunUserTask release_reset_and_reapply_config(Test& test) {
    co_await drive_reset_asserted(test, false);
    co_await wait_cycles(test, RESET_POST_RELEASE_CYCLES);
    co_await apply_reset_config(test);
    co_await wait_cycles(test, 2u);
    clear_post_reset_scoreboard_state(test);
    co_return;
}

TestBase::RunUserTask reset_and_reapply_config(Test& test,
                                               const unsigned low_cycles = RESET_LOW_CYCLES) {
    co_await drive_reset_asserted(test, true);
    co_await wait_cycles(test, low_cycles);
    co_await release_reset_and_reapply_config(test);
    co_return;
}

TestBase::RunUserTask sample_reset(Test& test, ResetSample& sample) {
    co_await test.core_intf.sample_status(sample.status);

    auto r = test.getCoRead();
    r.read(rst_n);
    r.read(uart_tx_o);
    r.read(uart_rx_i);
    co_await r;

    sample.rst_pin = (r.getNum(rst_n) & 1u) != 0u;
    sample.uart_tx_pin = (r.getNum(uart_tx_o) & 1u) != 0u;
    sample.uart_rx_pin = (r.getNum(uart_rx_i) & 1u) != 0u;
    co_return;
}

TestBase::RunUserTask wait_reset_status(Test& test,
                                        std::function<bool(const ResetSample&)> pred,
                                        std::string label,
                                        ResetSample* final_sample = nullptr,
                                        const unsigned timeout_cycles =
                                            RESET_STATUS_TIMEOUT_CYCLES) {
    ResetSample sample{};
    for (unsigned cycle = 0u; cycle < timeout_cycles; ++cycle) {
        co_await sample_reset(test, sample);
        if (pred(sample)) {
            if (final_sample != nullptr) {
                *final_sample = sample;
            }
            co_return;
        }
        co_await test.utils.clock(1, 1);
    }

    test.scb.note_fail("tc_reset: " + label + ": status wait timed out");
    if (final_sample != nullptr) {
        *final_sample = sample;
    }
    co_return;
}

void expect_idle_after_reset(Test& test,
                             const ResetSample& sample,
                             const std::string& label) {
    check_true(test, sample.rst_pin, label + ": rst_n is not released");
    check_true(test, sample.uart_tx_pin, label + ": uart_tx_o is not idle high");

    const UartCoreStatus& status = sample.status;
    check_true(test, status.rx_level == 0u, label + ": rx_fifo_level mismatch");
    check_true(test, status.tx_level == 0u, label + ": tx_fifo_level mismatch");
    check_true(test, status.rx_empty, label + ": rx_fifo_empty not asserted");
    check_true(test, status.tx_empty, label + ": tx_fifo_empty not asserted");
    check_true(test, !status.rx_full, label + ": rx_fifo_full asserted");
    check_true(test, !status.tx_full, label + ": tx_fifo_full asserted");
    check_true(test, !status.rx_busy, label + ": rx_busy asserted");
    check_true(test, !status.tx_busy, label + ": tx_busy asserted");
    check_true(test, !status.rx_byte_valid, label + ": rx_byte_valid asserted");
    check_true(test, status.tx_byte_ready, label + ": tx_byte_ready not asserted");
    check_true(test, !status.cts_blocked, label + ": cts_blocked asserted");
    check_true(test, !status.event_rx_overrun, label + ": event_rx_overrun asserted");
    check_true(test, !status.event_rx_frame_error, label + ": event_rx_frame_error asserted");
    check_true(test, !status.event_rx_parity_error, label + ": event_rx_parity_error asserted");
    check_true(test, !status.event_rx_break_detect, label + ": event_rx_break_detect asserted");
    check_true(test, !status.event_tx_done, label + ": event_tx_done asserted");
}

TestBase::RunUserTask expect_reset_idle_state(Test& test, const std::string& label) {
    ResetSample sample{};
    co_await wait_reset_status(test,
                               [](const ResetSample& s) {
                                   return s.rst_pin
                                       && s.uart_tx_pin
                                       && s.status.rx_level == 0u
                                       && s.status.tx_level == 0u
                                       && s.status.rx_empty
                                       && s.status.tx_empty
                                       && !s.status.rx_full
                                       && !s.status.tx_full
                                       && !s.status.rx_busy
                                       && !s.status.tx_busy
                                       && !s.status.rx_byte_valid
                                       && s.status.tx_byte_ready;
                               },
                               label,
                               &sample);
    expect_idle_after_reset(test, sample, label);
    co_return;
}

TestBase::RunUserTask wait_rx_level(Test& test,
                                    const unsigned level,
                                    std::string label) {
    co_await wait_reset_status(test,
                               [level](const ResetSample& sample) {
                                   return sample.status.rx_level == level
                                       && sample.status.rx_empty == (level == 0u)
                                       && sample.status.rx_byte_valid == (level != 0u);
                               },
                               label);
    co_return;
}

TestBase::RunUserTask wait_tx_level(Test& test,
                                    const unsigned level,
                                    std::string label) {
    co_await wait_reset_status(test,
                               [level](const ResetSample& sample) {
                                   return sample.status.tx_level == level
                                       && sample.status.tx_empty == (level == 0u)
                                       && sample.status.tx_byte_ready == (level < TX_FIFO_DEPTH);
                               },
                               label);
    co_return;
}

TestBase::RunUserTask wait_tx_busy(Test& test,
                                   std::string label,
                                   const unsigned timeout_cycles = HANDSHAKE_TIMEOUT_CYCLES) {
    co_await wait_reset_status(test,
                               [](const ResetSample& sample) {
                                   return sample.status.tx_busy;
                               },
                               label,
                               nullptr,
                               timeout_cycles);
    co_return;
}

TestBase::RunUserTask wait_rx_busy(Test& test,
                                   std::string label,
                                   const unsigned timeout_cycles = HANDSHAKE_TIMEOUT_CYCLES) {
    co_await wait_reset_status(test,
                               [](const ResetSample& sample) {
                                   return sample.status.rx_busy;
                               },
                               label,
                               nullptr,
                               timeout_cycles);
    co_return;
}

TestBase::RunUserTask send_rx_byte(Test& test,
                                   const std::uint8_t data,
                                   const vip::uart::UartParams& params) {
    const unsigned ticket = test.uart_peer_tx.enqueue_byte(uart_rx_port_name, data);
    co_await test.uart_peer_tx.wait_done(ticket);
    co_await wait_cycles(test, params.bit_clks * 4u);
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

TestBase::RunUserTask verify_no_rx_record_for(Test& test,
                                              const unsigned cycles,
                                              std::string label) {
    co_await test.core_intf.set_rx_ready(false);
    for (unsigned cycle = 0u; cycle < cycles; ++cycle) {
        UartCoreStatus status{};
        co_await test.core_intf.sample_status(status);
        if (status.rx_byte_valid || status.rx_level != 0u) {
            test.scb.note_fail("tc_reset: " + label + ": stale RX record visible after reset");
            co_return;
        }
        co_await test.utils.clock(1, 1);
    }
    co_return;
}

[[nodiscard]] bool event_counts_unchanged(const UartCoreEventCounts& before,
                                          const UartCoreEventCounts& after) {
    return before.rx_overrun == after.rx_overrun
        && before.rx_frame_error == after.rx_frame_error
        && before.rx_parity_error == after.rx_parity_error
        && before.rx_break_detect == after.rx_break_detect
        && before.tx_done == after.tx_done;
}

TestBase::RunUserTask verify_no_event_delta(Test& test,
                                            const UartCoreEventCounts& before,
                                            const unsigned cycles,
                                            std::string label) {
    for (unsigned cycle = 0u; cycle < cycles; ++cycle) {
        const UartCoreEventCounts after = test.core_intf.event_counts();
        if (!event_counts_unchanged(before, after)) {
            test.scb.note_fail("tc_reset: " + label + ": stale event count changed after reset");
            co_return;
        }
        co_await test.utils.clock(1, 1);
    }
    co_return;
}

TestBase::RunUserTask wait_tx_done_delta(Test& test,
                                         const UartCoreEventCounts& before,
                                         const std::uint64_t delta,
                                         std::string label,
                                         const unsigned timeout_cycles =
                                             RESET_STATUS_TIMEOUT_CYCLES) {
    const std::uint64_t target = before.tx_done + delta;
    for (unsigned cycle = 0u; cycle < timeout_cycles; ++cycle) {
        const std::uint64_t observed = test.core_intf.event_counts().tx_done;
        if (observed == target) {
            co_return;
        }
        if (observed > target) {
            break;
        }
        co_await test.utils.clock(1, 1);
    }

    check_true(test,
               test.core_intf.event_counts().tx_done == target,
               label + ": event_tx_done count mismatch");
    co_return;
}

TestBase::RunUserTask wait_tx_frame_count(Test& test,
                                          const std::size_t count,
                                          const unsigned timeout_cycles,
                                          std::string label) {
    for (unsigned cycle = 0u; cycle < timeout_cycles; ++cycle) {
        if (test.uart_peer_rx.observed_count(uart_tx_port_name) >= count) {
            co_return;
        }
        co_await test.utils.clock(1, 1);
    }

    test.scb.note_fail("tc_reset: " + label + ": UART TX frame wait timed out");
    co_return;
}

TestBase::RunUserTask verify_no_extra_tx_frames(Test& test,
                                                const std::size_t allowed_count,
                                                const unsigned cycles,
                                                std::string label) {
    for (unsigned cycle = 0u; cycle < cycles; ++cycle) {
        if (test.uart_peer_rx.observed_count(uart_tx_port_name) > allowed_count) {
            test.scb.note_fail("tc_reset: " + label + ": unexpected UART TX frame observed");
            co_return;
        }
        co_await test.utils.clock(1, 1);
    }
    co_return;
}

TestBase::RunUserTask check_uart_tx_idle_high_for(Test& test,
                                                  const unsigned cycles,
                                                  std::string label) {
    for (unsigned cycle = 0u; cycle < cycles; ++cycle) {
        auto r = test.getCoRead();
        r.read(uart_tx_o);
        co_await r;
        if ((r.getNum(uart_tx_o) & 1u) == 0u) {
            test.scb.note_fail("tc_reset: " + label + ": uart_tx_o left idle-high state");
            co_return;
        }
        co_await test.utils.clock(1, 1);
    }
    co_return;
}

void expect_tx_bytes(Test& test, const std::vector<std::uint8_t>& bytes) {
    test.scb_uart_stream.expect_bytes(uart_tx_port_name, bytes);
    test.scb_core.expect_tx_bytes(bytes);
}

void expect_aborted_tx_monitor_artifact(Test& test,
                                        const vip::uart::UartParams& params) {
    vip::uart::UartFrame artifact{};
    artifact.data = 0xffu;
    artifact.data_bits = params.data_bits;
    artifact.stop_bits = params.stop_bits;
    artifact.parity = params.parity;

    test.scb_uart_stream.set_strict_status_compare(false);
    test.scb_uart_stream.expect_frame(uart_tx_port_name, artifact);
}

void observe_tx_frames(Test& test,
                       std::size_t& next_index,
                       const std::size_t expected_total,
                       const std::string& label) {
    const auto history = test.uart_peer_rx.get_history(uart_tx_port_name);
    if (history.size() < expected_total) {
        test.scb.note_fail("tc_reset: " + label + ": missing UART TX frames");
    }
    if (history.size() > expected_total) {
        test.scb.note_fail("tc_reset: " + label + ": extra UART TX frames observed");
    }

    const std::size_t stop = std::min(history.size(), expected_total);
    for (std::size_t i = next_index; i < stop; ++i) {
        test.scb_core.observe_uart_tx_frame(history.at(i));
    }
    next_index = stop;
}

TestBase::RunUserTask send_tx_expect(Test& test,
                                     const std::uint8_t data,
                                     const vip::uart::UartParams& params,
                                     std::string label) {
    test.uart_peer_rx.clear_history(uart_tx_port_name);
    std::size_t observed_tx = 0u;
    const UartCoreEventCounts before = test.core_intf.event_counts();

    expect_tx_bytes(test, {data});
    co_await test.core_intf.push_tx_byte(data);
    co_await wait_tx_frame_count(test, 1u, tx_timeout_for(params, 1u), label);
    observe_tx_frames(test, observed_tx, 1u, label);
    co_await wait_tx_done_delta(test, before, 1u, label);
    co_await test.core_intf.wait_tx_idle(params.frame_clks() + HANDSHAKE_TIMEOUT_CYCLES);
    co_return;
}

TestBase::RunUserTask subcase_reset_while_idle(Test& test) {
    log_subcase("reset while idle");
    co_await tc_local_reset(test);
    co_await apply_reset_config(test);
    co_await expect_reset_idle_state(test, "idle pre-reset");

    co_await reset_and_reapply_config(test);
    co_await expect_reset_idle_state(test, "idle post-reset");

    const vip::uart::UartParams params = make_uart_params();
    co_await send_rx_byte(test, 0x55u, params);
    co_await pop_rx_expect(test, 0x55u, "idle recovery RX");
    co_await send_tx_expect(test, 0xa5u, params, "idle recovery TX");
    co_return;
}

TestBase::RunUserTask subcase_reset_with_nonempty_rx_fifo(Test& test) {
    log_subcase("reset with non-empty RX FIFO");
    co_await tc_local_reset(test);
    co_await apply_reset_config(test);
    co_await test.core_intf.set_rx_ready(false);

    const vip::uart::UartParams params = make_uart_params();
    const std::vector<std::uint8_t> stale = {0x11u, 0x22u, 0x33u, 0x44u};
    for (std::size_t i = 0u; i < stale.size(); ++i) {
        co_await send_rx_byte(test, stale.at(i), params);
        co_await wait_rx_level(test, static_cast<unsigned>(i + 1u), "RX FIFO fill before reset");
    }

    co_await reset_and_reapply_config(test);
    co_await expect_reset_idle_state(test, "non-empty RX FIFO post-reset");
    co_await verify_no_rx_record_for(test, NO_STALE_RX_CYCLES, "non-empty RX FIFO post-reset");

    co_await send_rx_byte(test, 0x5au, params);
    co_await pop_rx_expect(test, 0x5au, "non-empty RX FIFO recovery RX");
    co_return;
}

TestBase::RunUserTask subcase_reset_with_nonempty_tx_fifo(Test& test) {
    log_subcase("reset with non-empty TX FIFO");
    co_await tc_local_reset(test);
    co_await apply_reset_config(test, true, false);
    test.uart_peer_rx.clear_history(uart_tx_port_name);

    const vip::uart::UartParams params = make_uart_params();
    const std::vector<std::uint8_t> stale = {0x61u, 0x62u, 0x63u, 0x64u};
    for (std::size_t i = 0u; i < stale.size(); ++i) {
        co_await test.core_intf.push_tx_byte(stale.at(i));
        co_await wait_tx_level(test, static_cast<unsigned>(i + 1u), "TX FIFO fill before reset");
    }

    co_await reset_and_reapply_config(test);
    co_await expect_reset_idle_state(test, "non-empty TX FIFO post-reset");

    const UartCoreEventCounts after_reset = test.core_intf.event_counts();
    co_await verify_no_extra_tx_frames(test,
                                       0u,
                                       params.frame_clks() * 2u,
                                       "non-empty TX FIFO post-reset");
    co_await verify_no_event_delta(test,
                                   after_reset,
                                   params.frame_clks() * 2u,
                                   "non-empty TX FIFO post-reset");

    co_await send_tx_expect(test, 0x77u, params, "non-empty TX FIFO recovery TX");
    co_return;
}

TestBase::RunUserTask subcase_reset_while_tx_active(Test& test) {
    log_subcase("reset while TX character active");
    co_await tc_local_reset(test);
    co_await apply_reset_config(test);
    test.uart_peer_rx.clear_history(uart_tx_port_name);

    const vip::uart::UartParams params = make_uart_params();
    co_await test.core_intf.push_tx_byte(0xc1u);
    co_await test.core_intf.push_tx_byte(0xc2u);
    co_await wait_tx_busy(test, "active TX launch");

    set_tx_monitor_strict(test, false);
    expect_aborted_tx_monitor_artifact(test, params);
    co_await drive_reset_asserted(test, true);
    co_await wait_cycles(test, 2u);

    ResetSample during_reset{};
    co_await sample_reset(test, during_reset);
    check_true(test, !during_reset.rst_pin, "active TX: rst_n did not assert");
    check_true(test, during_reset.uart_tx_pin, "active TX: uart_tx_o not forced idle high during reset");
    check_true(test, !during_reset.status.tx_busy, "active TX: tx_busy did not clear during reset");

    co_await wait_cycles(test, RESET_LOW_CYCLES);
    co_await release_reset_and_reapply_config(test);
    co_await expect_reset_idle_state(test, "active TX post-reset");

    const UartCoreEventCounts after_reset = test.core_intf.event_counts();
    co_await check_uart_tx_idle_high_for(test, params.frame_clks(), "active TX abort post-reset");
    co_await verify_no_event_delta(test, after_reset, params.frame_clks(), "active TX abort post-reset");

    test.scb_uart_stream.reset_case();
    test.scb_uart_stream.set_strict_status_compare(true);
    test.scb_uart_rules.reset_case();
    test.uart_peer_rx.clear_history(uart_tx_port_name);
    set_tx_monitor_strict(test, true);

    co_await send_tx_expect(test, 0x3cu, params, "active TX recovery TX");
    co_return;
}

TestBase::RunUserTask subcase_reset_while_rx_active(Test& test) {
    log_subcase("reset while RX character active");
    co_await tc_local_reset(test);
    co_await apply_reset_config(test);

    const vip::uart::UartParams params = make_uart_params();
    const unsigned ticket = test.uart_peer_tx.enqueue_byte(uart_rx_port_name, 0xa6u);
    co_await wait_rx_busy(test, "active RX start");

    co_await drive_reset_asserted(test, true);
    co_await wait_cycles(test, 2u);

    ResetSample during_reset{};
    co_await sample_reset(test, during_reset);
    check_true(test, !during_reset.rst_pin, "active RX: rst_n did not assert");
    check_true(test, !during_reset.status.rx_busy, "active RX: rx_busy did not clear during reset");

    co_await test.uart_peer_tx.wait_done(ticket);
    co_await wait_cycles(test, params.bit_clks * 2u);
    co_await release_reset_and_reapply_config(test);
    co_await expect_reset_idle_state(test, "active RX post-reset");

    const UartCoreEventCounts after_reset = test.core_intf.event_counts();
    co_await verify_no_rx_record_for(test, NO_STALE_RX_CYCLES, "active RX post-reset");
    co_await verify_no_event_delta(test, after_reset, params.frame_clks(), "active RX post-reset");

    co_await send_rx_byte(test, 0x6au, params);
    co_await pop_rx_expect(test, 0x6au, "active RX recovery RX");
    co_return;
}

TestBase::RunUserTask subcase_reset_during_rx_low_condition(Test& test) {
    log_subcase("reset during RX low/start-like condition");
    co_await tc_local_reset(test);
    co_await apply_reset_config(test);

    const vip::uart::UartParams params = make_uart_params();
    test.uart_peer_tx.arm_next_framing_error(uart_rx_port_name);
    const unsigned ticket = test.uart_peer_tx.enqueue_byte(uart_rx_port_name, 0x00u);
    co_await wait_rx_busy(test, "RX low/start-like condition");

    ResetSample before_reset{};
    co_await sample_reset(test, before_reset);
    check_true(test, !before_reset.uart_rx_pin, "RX low/start-like condition: uart_rx_i is not low");

    co_await drive_reset_asserted(test, true);
    co_await wait_cycles(test, 2u);
    co_await test.uart_peer_tx.wait_done(ticket);
    co_await wait_cycles(test, params.bit_clks * 2u);

    ResetSample line_idle{};
    co_await sample_reset(test, line_idle);
    check_true(test, line_idle.uart_rx_pin, "RX low/start-like condition: uart_rx_i not idle before reset release");

    co_await release_reset_and_reapply_config(test);
    co_await expect_reset_idle_state(test, "RX low/start-like post-reset");

    const UartCoreEventCounts after_reset = test.core_intf.event_counts();
    co_await verify_no_rx_record_for(test, NO_STALE_RX_CYCLES, "RX low/start-like post-reset");
    co_await verify_no_event_delta(test, after_reset, params.frame_clks(), "RX low/start-like post-reset");

    co_await send_rx_byte(test, 0x99u, params);
    co_await pop_rx_expect(test, 0x99u, "RX low/start-like recovery RX");
    co_return;
}

TestBase::RunUserTask subcase_repeated_reset_recovery(Test& test) {
    log_subcase("repeated reset recovery");
    co_await tc_local_reset(test);
    co_await apply_reset_config(test);

    const vip::uart::UartParams params = make_uart_params();
    const std::vector<unsigned> reset_lengths = {3u, 7u, 11u};
    for (std::size_t i = 0u; i < reset_lengths.size(); ++i) {
        vip::common::log_line("tc_reset",
                              "INFO",
                              "subcase repeated reset iteration " + std::to_string(i));
        co_await reset_and_reapply_config(test, reset_lengths.at(i));
        co_await expect_reset_idle_state(test,
                                         "repeated reset iteration " + std::to_string(i));

        const std::uint8_t rx_data = static_cast<std::uint8_t>(0x10u + i);
        const std::uint8_t tx_data = static_cast<std::uint8_t>(0x20u + i);
        co_await send_rx_byte(test, rx_data, params);
        co_await pop_rx_expect(test,
                               rx_data,
                               "repeated reset iteration " + std::to_string(i) + " RX");
        co_await send_tx_expect(test,
                                tx_data,
                                params,
                                "repeated reset iteration " + std::to_string(i) + " TX");
    }
    co_return;
}

} // namespace

TestBase::RunUserTask tc_reset(Test& test) {

    co_await subcase_reset_while_idle(test);
    co_await subcase_reset_with_nonempty_rx_fifo(test);
    co_await subcase_reset_with_nonempty_tx_fifo(test);
    co_await subcase_reset_while_tx_active(test);
    co_await subcase_reset_while_rx_active(test);
    co_await subcase_reset_during_rx_low_condition(test);
    co_await subcase_repeated_reset_recovery(test);

    if (!test.scb.case_has_failures()) {
        test.scb.note_pass("tc_reset reset behavior completed");
    }

    co_return;
}

} // namespace test

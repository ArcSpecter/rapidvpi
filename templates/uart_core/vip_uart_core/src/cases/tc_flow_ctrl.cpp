#include "tc_flow_ctrl.hpp"

// DUT parameter requirements for this testcase:
//   HAS_RTS_CTS        = 1'b1
//   RTS_ACTIVE_LOW     = 1'b1
//   CTS_ACTIVE_LOW     = 1'b1
//   RTS_DEASSERT_LEVEL = RX_FIFO_DEPTH - 2
//   RTS_ASSERT_LEVEL   = RX_FIFO_DEPTH / 2
// The default HAS_RTS_CTS=0 uart_core build only verifies tie-off behavior and
// is not sufficient for tc_flow_ctrl.

#include <cstddef>
#include <cstdint>
#include <deque>
#include <functional>
#include <string>
#include <vector>

#include "tc_utils.hpp"

#include "../pindefs.hpp"

#include "vip_common/common/logger.hpp"

namespace test {
namespace {

static constexpr bool CTS_ACTIVE_LOW = true;
static constexpr bool RTS_ACTIVE_LOW = true;
static constexpr unsigned CTS_SYNC_SETTLE_CYCLES = 6u;
static constexpr unsigned FLOW_STATUS_TIMEOUT_CYCLES = BASIC_UART_BIT_CLKS * 64u;
static constexpr unsigned FLOW_RTS_DEASSERT_LEVEL = RX_FIFO_DEPTH - 2u;
static constexpr unsigned FLOW_RTS_ASSERT_LEVEL = RX_FIFO_DEPTH / 2u;

struct FlowSample {
    UartCoreStatus status{};
    bool uart_rts_pin = false;
    bool uart_tx_pin = true;
};

[[nodiscard]] bool cts_pin_for_active(const bool active) {
    return CTS_ACTIVE_LOW ? !active : active;
}

[[nodiscard]] bool rts_pin_for_active(const bool active) {
    return RTS_ACTIVE_LOW ? !active : active;
}

void log_subcase(const std::string& name) {
    vip::common::log_line("tc_flow_ctrl", "INFO", "subcase " + name);
}

void check_true(Test& test, const bool condition, const std::string& msg) {
    if (!condition) {
        test.scb.note_fail("tc_flow_ctrl: " + msg);
    }
}

TestBase::RunUserTask wait_cycles(Test& test, const unsigned cycles) {
    if (cycles != 0u) {
        co_await test.utils.clock(static_cast<int>(cycles), 1);
    }
    co_return;
}

TestBase::RunUserTask sample_flow(Test& test, FlowSample& sample) {
    co_await test.core_intf.sample_status(sample.status);

    auto r = test.getCoRead();
    r.read(uart_rts_o);
    r.read(uart_tx_o);
    co_await r;

    sample.uart_rts_pin = (r.getNum(uart_rts_o) & 1u) != 0u;
    sample.uart_tx_pin = (r.getNum(uart_tx_o) & 1u) != 0u;
    co_return;
}

TestBase::RunUserTask wait_flow_status(Test& test,
                                       std::function<bool(const FlowSample&)> pred,
                                       std::string label,
                                       FlowSample* final_sample = nullptr,
                                       const unsigned timeout_cycles =
                                           FLOW_STATUS_TIMEOUT_CYCLES) {
    FlowSample sample{};
    for (unsigned cycle = 0u; cycle < timeout_cycles; ++cycle) {
        co_await sample_flow(test, sample);
        if (pred(sample)) {
            if (final_sample != nullptr) {
                *final_sample = sample;
            }
            co_return;
        }
        co_await test.utils.clock(1, 1);
    }

    test.scb.note_fail("tc_flow_ctrl: " + label + ": status wait timed out");
    if (final_sample != nullptr) {
        *final_sample = sample;
    }
    co_return;
}

void expect_cts_active(Test& test,
                       const FlowSample& sample,
                       const bool expected,
                       const std::string& label) {
    check_true(test, sample.status.cts_active == expected, label + ": cts_active mismatch");
}

void expect_cts_blocked(Test& test,
                        const FlowSample& sample,
                        const bool expected,
                        const std::string& label) {
    check_true(test, sample.status.cts_blocked == expected, label + ": cts_blocked mismatch");
}

void expect_rts_active(Test& test,
                       const FlowSample& sample,
                       const bool expected,
                       const std::string& label) {
    check_true(test, sample.status.rts_active == expected, label + ": rts_active mismatch");
    check_true(test,
               sample.uart_rts_pin == rts_pin_for_active(expected),
               label + ": uart_rts_o physical polarity mismatch");
}

TestBase::RunUserTask wait_expect_rts_active(Test& test,
                                             const bool expected,
                                             std::string label) {
    co_await wait_flow_status(test,
                              [expected](const FlowSample& sample) {
                                  return sample.status.rts_active == expected
                                      && sample.uart_rts_pin == rts_pin_for_active(expected);
                              },
                              label);
    co_return;
}

TestBase::RunUserTask drive_cts_active(Test& test, const bool active) {
    co_await test.utils.clock_to_write(1, 0);
    co_await test.uart_peer_rx.drive_cts_now(uart_tx_port_name, active);
    co_await wait_cycles(test, CTS_SYNC_SETTLE_CYCLES);

    auto r = test.getCoRead();
    r.read(uart_cts_i);
    co_await r;
    const bool physical = (r.getNum(uart_cts_i) & 1u) != 0u;
    check_true(test,
               physical == cts_pin_for_active(active),
               std::string("uart_cts_i physical polarity mismatch while driving CTS ")
                   + (active ? "active" : "inactive"));
    co_return;
}

TestBase::RunUserTask apply_flow_config(Test& test,
                                        const bool hw_flow_enable = true,
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
                            hw_flow_enable);
    co_await tc_apply_uart_config(test, cfg, make_uart_params());
    co_await test.utils.clock(2, 1);
    co_return;
}

[[nodiscard]] unsigned tx_timeout_for(const vip::uart::UartParams& params,
                                      const std::size_t frames) {
    return (params.frame_clks() * static_cast<unsigned>(frames + 2u))
        + (HANDSHAKE_TIMEOUT_CYCLES * 4u);
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

    test.scb.note_fail("tc_flow_ctrl: " + label + ": UART TX frame wait timed out");
    co_return;
}

TestBase::RunUserTask verify_no_extra_tx_frames(Test& test,
                                                const std::size_t allowed_count,
                                                const unsigned cycles,
                                                std::string label) {
    for (unsigned cycle = 0u; cycle < cycles; ++cycle) {
        if (test.uart_peer_rx.observed_count(uart_tx_port_name) > allowed_count) {
            test.scb.note_fail("tc_flow_ctrl: " + label + ": unexpected UART TX frame observed");
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

void observe_tx_frames(Test& test,
                       std::size_t& next_index,
                       const std::size_t expected_total,
                       const std::string& label) {
    const auto history = test.uart_peer_rx.get_history(uart_tx_port_name);
    if (history.size() < expected_total) {
        test.scb.note_fail("tc_flow_ctrl: " + label + ": missing UART TX frames");
    }
    if (history.size() > expected_total) {
        test.scb.note_fail("tc_flow_ctrl: " + label + ": extra UART TX frames observed");
    }

    const std::size_t stop = history.size() < expected_total ? history.size() : expected_total;
    for (std::size_t i = next_index; i < stop; ++i) {
        test.scb_core.observe_uart_tx_frame(history.at(i));
    }
    next_index = stop;
}

TestBase::RunUserTask wait_tx_done_delta(Test& test,
                                         const UartCoreEventCounts& before,
                                         const std::uint64_t delta,
                                         std::string label,
                                         const unsigned timeout_cycles =
                                             FLOW_STATUS_TIMEOUT_CYCLES) {
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

TestBase::RunUserTask wait_rx_level(Test& test,
                                    const unsigned level,
                                    std::string label) {
    co_await wait_flow_status(test,
                              [level](const FlowSample& sample) {
                                  return sample.status.rx_level == level
                                      && sample.status.rx_byte_valid == (level != 0u);
                              },
                              label);
    co_return;
}

TestBase::RunUserTask send_rx_and_expect_level(Test& test,
                                               const std::uint8_t data,
                                               const unsigned level,
                                               std::deque<std::uint8_t>& queued,
                                               const vip::uart::UartParams& params,
                                               std::string label) {
    co_await send_rx_byte(test, data, params);
    queued.push_back(data);
    co_await wait_rx_level(test, level, label);
    co_return;
}

TestBase::RunUserTask pop_queued_rx(Test& test,
                                    std::deque<std::uint8_t>& queued,
                                    const unsigned expected_level,
                                    std::string label) {
    if (queued.empty()) {
        test.scb.note_fail("tc_flow_ctrl: " + label + ": queued RX model is empty");
        co_return;
    }

    const std::uint8_t data = queued.front();
    queued.pop_front();
    co_await pop_rx_expect(test, data, label);
    co_await wait_rx_level(test, expected_level, label + " level");
    co_return;
}

TestBase::RunUserTask subcase_reset_default_status(Test& test) {
    log_subcase("reset/default flow-control status");
    co_await tc_local_reset(test);
    co_await apply_flow_config(test, true, true, true);
    co_await drive_cts_active(test, true);

    FlowSample sample{};
    co_await sample_flow(test, sample);
    expect_cts_active(test, sample, true, "reset/default");
    expect_cts_blocked(test, sample, false, "reset/default");
    expect_rts_active(test, sample, true, "reset/default");
    co_return;
}

TestBase::RunUserTask subcase_cfg_hw_flow_disable_ignores_cts(Test& test) {
    log_subcase("cfg_hw_flow_enable=0 ignores CTS");
    co_await tc_local_reset(test);
    co_await apply_flow_config(test, false, true, true);
    co_await drive_cts_active(test, false);

    test.uart_peer_rx.clear_history(uart_tx_port_name);
    std::size_t observed_tx = 0u;
    const vip::uart::UartParams params = make_uart_params();
    const UartCoreEventCounts before = test.core_intf.event_counts();

    expect_tx_bytes(test, {0x41u});
    co_await test.core_intf.push_tx_byte(0x41u);
    co_await wait_tx_frame_count(test, 1u, tx_timeout_for(params, 1u), "flow disabled TX");
    observe_tx_frames(test, observed_tx, 1u, "flow disabled TX");
    co_await wait_tx_done_delta(test, before, 1u, "flow disabled TX");
    co_await test.core_intf.wait_tx_idle(params.frame_clks() + HANDSHAKE_TIMEOUT_CYCLES);

    FlowSample sample{};
    co_await sample_flow(test, sample);
    expect_cts_blocked(test, sample, false, "flow disabled TX");
    co_return;
}

TestBase::RunUserTask subcase_cts_blocks_and_releases_tx(Test& test) {
    log_subcase("CTS inactive blocks TX launch");
    co_await tc_local_reset(test);
    co_await apply_flow_config(test, true, true, true);
    co_await drive_cts_active(test, false);

    FlowSample sample{};
    co_await sample_flow(test, sample);
    expect_cts_active(test, sample, false, "CTS inactive pre-push");

    test.uart_peer_rx.clear_history(uart_tx_port_name);
    const vip::uart::UartParams params = make_uart_params();
    const UartCoreEventCounts before = test.core_intf.event_counts();

    co_await test.core_intf.push_tx_byte(0x55u);
    co_await wait_flow_status(test,
                              [](const FlowSample& s) {
                                  return s.status.tx_level == 1u
                                      && !s.status.tx_busy
                                      && s.status.cts_blocked
                                      && s.uart_tx_pin;
                              },
                              "CTS blocked queued TX",
                              &sample);
    check_true(test, sample.status.tx_byte_ready, "CTS blocked queued TX: tx_byte_ready deasserted unexpectedly");
    co_await verify_no_extra_tx_frames(test, 0u, params.bit_clks * 4u, "CTS blocked queued TX");
    co_await wait_tx_done_delta(test, before, 0u, "CTS blocked queued TX", params.bit_clks * 4u);

    log_subcase("CTS release allows blocked TX launch");
    std::size_t observed_tx = 0u;
    expect_tx_bytes(test, {0x55u});
    co_await drive_cts_active(test, true);
    co_await wait_flow_status(test,
                              [](const FlowSample& s) {
                                  return s.status.cts_active && !s.status.cts_blocked;
                              },
                              "CTS release status");
    co_await wait_tx_frame_count(test, 1u, tx_timeout_for(params, 1u), "CTS release TX");
    observe_tx_frames(test, observed_tx, 1u, "CTS release TX");
    co_await wait_tx_done_delta(test, before, 1u, "CTS release TX");
    co_await test.core_intf.wait_tx_idle(params.frame_clks() + HANDSHAKE_TIMEOUT_CYCLES);
    co_await wait_flow_status(test,
                              [](const FlowSample& s) {
                                  return s.status.tx_level == 0u
                                      && s.status.tx_empty
                                      && !s.status.cts_blocked;
                              },
                              "CTS release final idle");
    co_return;
}

TestBase::RunUserTask subcase_cts_does_not_interrupt_active_tx(Test& test) {
    log_subcase("CTS inactive does not interrupt active TX character");
    co_await tc_local_reset(test);
    co_await apply_flow_config(test, true, true, true);
    co_await drive_cts_active(test, true);

    test.uart_peer_rx.clear_history(uart_tx_port_name);
    std::size_t observed_tx = 0u;
    const vip::uart::UartParams params = make_uart_params();
    const UartCoreEventCounts before = test.core_intf.event_counts();

    expect_tx_bytes(test, {0xa1u, 0xb2u});
    co_await test.core_intf.push_tx_byte(0xa1u);
    co_await test.core_intf.push_tx_byte(0xb2u);
    co_await wait_flow_status(test,
                              [](const FlowSample& s) { return s.status.tx_busy; },
                              "first TX launch");

    co_await drive_cts_active(test, false);
    co_await wait_tx_frame_count(test, 1u, tx_timeout_for(params, 1u), "first TX while CTS drops");
    observe_tx_frames(test, observed_tx, 1u, "first TX while CTS drops");
    co_await wait_tx_done_delta(test, before, 1u, "first TX while CTS drops");

    FlowSample sample{};
    co_await wait_flow_status(test,
                              [](const FlowSample& s) {
                                  return s.status.tx_level == 1u
                                      && !s.status.tx_busy
                                      && s.status.cts_blocked;
                              },
                              "second TX blocked after first completion",
                              &sample);
    expect_cts_active(test, sample, false, "second TX blocked after first completion");
    co_await verify_no_extra_tx_frames(test,
                                       1u,
                                       params.bit_clks * 4u,
                                       "second TX remains blocked");

    co_await drive_cts_active(test, true);
    co_await wait_tx_frame_count(test, 2u, tx_timeout_for(params, 1u), "second TX after CTS release");
    observe_tx_frames(test, observed_tx, 2u, "second TX after CTS release");
    co_await wait_tx_done_delta(test, before, 2u, "second TX after CTS release");
    co_await test.core_intf.wait_tx_idle(params.frame_clks() + HANDSHAKE_TIMEOUT_CYCLES);
    co_await wait_flow_status(test,
                              [](const FlowSample& s) {
                                  return s.status.tx_level == 0u
                                      && s.status.tx_empty
                                      && !s.status.cts_blocked;
                              },
                              "CTS interrupt final idle");
    co_return;
}

TestBase::RunUserTask subcase_cts_blocked_only_when_waiting(Test& test) {
    log_subcase("cts_blocked only asserts when a byte is actually waiting");
    co_await tc_local_reset(test);
    co_await apply_flow_config(test, true, true, true);
    co_await drive_cts_active(test, false);
    test.uart_peer_rx.clear_history(uart_tx_port_name);

    FlowSample sample{};
    co_await wait_flow_status(test,
                              [](const FlowSample& s) {
                                  return !s.status.cts_active
                                      && !s.status.cts_blocked
                                      && !s.status.tx_busy
                                      && s.status.tx_empty
                                      && s.status.tx_level == 0u;
                              },
                              "inactive CTS with empty TX",
                              &sample);
    expect_cts_active(test, sample, false, "inactive CTS with empty TX");
    expect_cts_blocked(test, sample, false, "inactive CTS with empty TX");

    co_await test.core_intf.push_tx_byte(0x66u);
    co_await wait_flow_status(test,
                              [](const FlowSample& s) {
                                  return s.status.tx_level == 1u
                                      && s.status.cts_blocked
                                      && !s.status.tx_busy;
                              },
                              "queued byte blocked by CTS");

    co_await test.core_intf.pulse_tx_fifo_clear();
    co_await wait_flow_status(test,
                              [](const FlowSample& s) {
                                  return s.status.tx_level == 0u
                                      && s.status.tx_empty
                                      && !s.status.cts_blocked
                                      && !s.status.tx_busy;
                              },
                              "blocked byte cleared");
    co_await verify_no_extra_tx_frames(test, 0u, make_uart_params().bit_clks * 4u, "blocked byte cleared");
    co_return;
}

TestBase::RunUserTask subcase_rts_deasserts_and_reasserts(Test& test) {
    log_subcase("RTS deasserts at RX high-water threshold");
    co_await tc_local_reset(test);
    co_await apply_flow_config(test, true, true, true);
    co_await drive_cts_active(test, true);
    co_await test.core_intf.set_rx_ready(false);
    co_await test.core_intf.pulse_rx_fifo_clear();
    co_await wait_rx_level(test, 0u, "RTS threshold empty start");

    co_await wait_expect_rts_active(test, true, "RTS threshold empty start");

    const vip::uart::UartParams params = make_uart_params();
    std::deque<std::uint8_t> queued;
    for (unsigned i = 0u; i < FLOW_RTS_DEASSERT_LEVEL; ++i) {
        const unsigned level = i + 1u;
        co_await send_rx_and_expect_level(test,
                                          static_cast<std::uint8_t>(0x20u + i),
                                          level,
                                          queued,
                                          params,
                                          "RTS fill level " + std::to_string(level));
        co_await wait_expect_rts_active(test,
                                        level < FLOW_RTS_DEASSERT_LEVEL,
                                        "RTS fill level " + std::to_string(level));
    }

    log_subcase("RTS reasserts after RX FIFO drains");
    while (!queued.empty()) {
        const unsigned next_level = static_cast<unsigned>(queued.size() - 1u);
        co_await pop_queued_rx(test,
                               queued,
                               next_level,
                               "RTS drain pop level " + std::to_string(next_level));
        co_await wait_expect_rts_active(test,
                                        next_level <= FLOW_RTS_ASSERT_LEVEL,
                                        "RTS drain level " + std::to_string(next_level));
    }
    co_return;
}

TestBase::RunUserTask subcase_rts_hysteresis(Test& test) {
    log_subcase("RTS hysteresis stability between thresholds");
    co_await tc_local_reset(test);
    co_await apply_flow_config(test, true, true, true);
    co_await drive_cts_active(test, true);
    co_await test.core_intf.set_rx_ready(false);

    const vip::uart::UartParams params = make_uart_params();
    std::deque<std::uint8_t> queued;
    std::uint8_t next_data = 0x60u;

    auto send_to_level = [&](const unsigned target_level, const std::string& label)
        -> TestBase::RunUserTask {
        while (queued.size() < target_level) {
            const unsigned level = static_cast<unsigned>(queued.size() + 1u);
            co_await send_rx_and_expect_level(test, next_data, level, queued, params, label);
            next_data = static_cast<std::uint8_t>(next_data + 1u);
        }
        co_return;
    };

    auto pop_to_level = [&](const unsigned target_level, const std::string& label)
        -> TestBase::RunUserTask {
        while (queued.size() > target_level) {
            const unsigned level = static_cast<unsigned>(queued.size() - 1u);
            co_await pop_queued_rx(test, queued, level, label);
        }
        co_return;
    };

    co_await send_to_level(FLOW_RTS_DEASSERT_LEVEL, "hysteresis fill to deassert");
    co_await wait_expect_rts_active(test, false, "hysteresis at deassert level");

    co_await pop_to_level(FLOW_RTS_ASSERT_LEVEL + 2u, "hysteresis pop above assert");
    co_await wait_expect_rts_active(test, false, "hysteresis above assert after pop");

    co_await send_to_level(FLOW_RTS_DEASSERT_LEVEL - 1u, "hysteresis refill below deassert while inactive");
    co_await wait_expect_rts_active(test, false, "hysteresis below deassert while inactive");

    co_await pop_to_level(FLOW_RTS_ASSERT_LEVEL, "hysteresis pop to assert");
    co_await wait_expect_rts_active(test, true, "hysteresis at assert level");

    co_await send_to_level(FLOW_RTS_DEASSERT_LEVEL - 1u, "hysteresis refill below deassert while active");
    co_await wait_expect_rts_active(test, true, "hysteresis below deassert while active");

    co_await send_to_level(FLOW_RTS_DEASSERT_LEVEL, "hysteresis refill to deassert");
    co_await wait_expect_rts_active(test, false, "hysteresis final deassert");
    co_return;
}

TestBase::RunUserTask subcase_rx_tx_independence(Test& test) {
    log_subcase("RX/TX independence under flow control");
    co_await tc_local_reset(test);
    co_await apply_flow_config(test, true, true, true);
    co_await drive_cts_active(test, false);
    test.uart_peer_rx.clear_history(uart_tx_port_name);

    const vip::uart::UartParams params = make_uart_params();
    std::size_t observed_tx = 0u;
    UartCoreEventCounts before = test.core_intf.event_counts();

    co_await test.core_intf.push_tx_byte(0xd5u);
    co_await wait_flow_status(test,
                              [](const FlowSample& s) {
                                  return s.status.tx_level == 1u
                                      && s.status.cts_blocked
                                      && !s.status.tx_busy;
                              },
                              "independence TX blocked by CTS");

    co_await send_rx_byte(test, 0x5du, params);
    co_await pop_rx_expect(test, 0x5du, "independence RX while TX blocked");

    expect_tx_bytes(test, {0xd5u});
    co_await drive_cts_active(test, true);
    co_await wait_tx_frame_count(test, 1u, tx_timeout_for(params, 1u), "independence release CTS");
    observe_tx_frames(test, observed_tx, 1u, "independence release CTS");
    co_await wait_tx_done_delta(test, before, 1u, "independence release CTS");
    co_await test.core_intf.wait_tx_idle(params.frame_clks() + HANDSHAKE_TIMEOUT_CYCLES);

    co_await tc_local_reset(test);
    co_await apply_flow_config(test, true, true, true);
    co_await drive_cts_active(test, true);
    co_await test.core_intf.set_rx_ready(false);

    std::deque<std::uint8_t> queued;
    for (unsigned i = 0u; i < FLOW_RTS_DEASSERT_LEVEL; ++i) {
        co_await send_rx_and_expect_level(test,
                                          static_cast<std::uint8_t>(0xc0u + i),
                                          i + 1u,
                                          queued,
                                          params,
                                          "independence RTS fill");
    }

    FlowSample sample{};
    co_await sample_flow(test, sample);
    expect_rts_active(test, sample, false, "independence RTS inactive");

    test.uart_peer_rx.clear_history(uart_tx_port_name);
    observed_tx = 0u;
    before = test.core_intf.event_counts();
    expect_tx_bytes(test, {0xe7u});
    co_await test.core_intf.push_tx_byte(0xe7u);
    co_await wait_tx_frame_count(test, 1u, tx_timeout_for(params, 1u), "independence TX while RTS inactive");
    observe_tx_frames(test, observed_tx, 1u, "independence TX while RTS inactive");
    co_await wait_tx_done_delta(test, before, 1u, "independence TX while RTS inactive");
    co_return;
}

} // namespace

TestBase::RunUserTask tc_flow_ctrl(Test& test) {

    co_await subcase_reset_default_status(test);
    co_await subcase_cfg_hw_flow_disable_ignores_cts(test);
    co_await subcase_cts_blocks_and_releases_tx(test);
    co_await subcase_cts_does_not_interrupt_active_tx(test);
    co_await subcase_cts_blocked_only_when_waiting(test);
    co_await subcase_rts_deasserts_and_reasserts(test);
    co_await subcase_rts_hysteresis(test);
    co_await subcase_rx_tx_independence(test);

    if (!test.scb.case_has_failures()) {
        test.scb.note_pass("tc_flow_ctrl RTS/CTS behavior completed");
    }

    co_return;
}

} // namespace test

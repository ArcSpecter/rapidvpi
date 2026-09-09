#include "tc_stress_no_cts.hpp"

// DUT parameter requirements for this testcase:
//   HAS_RTS_CTS = 1'b0, the default no-CTS uart_core build.
// No testcase-specific DUT generic overrides are required. The runtime config
// keeps cfg_hw_flow_enable=0, so this case does not verify RTS/CTS behavior.

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

static constexpr unsigned STRESS_STATUS_TIMEOUT_CYCLES = BASIC_UART_BIT_CLKS * 96u;

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

[[nodiscard]] unsigned data_bits_from_cfg(const unsigned mode) {
    return 5u + (mode & 0x3u);
}

[[nodiscard]] unsigned stop_bits_from_cfg(const unsigned mode) {
    return mode == UART_STOP_2 ? 2u : 1u;
}

[[nodiscard]] std::uint8_t data_mask(const unsigned bits) {
    if (bits >= 8u) {
        return 0xffu;
    }
    return static_cast<std::uint8_t>((1u << bits) - 1u);
}

void log_subcase(const std::string& name) {
    vip::common::log_line("tc_stress_no_cts", "INFO", "subcase " + name);
}

void check_true(Test& test, const bool condition, const std::string& msg) {
    if (!condition) {
        test.scb.note_fail("tc_stress_no_cts: " + msg);
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

TestBase::RunUserTask apply_stress_config(Test& test,
                                          const unsigned parity_mode = UART_PARITY_NONE,
                                          const unsigned stop_mode = UART_STOP_1,
                                          const unsigned data_mode = UART_DATA_8,
                                          const bool rx_enable = true,
                                          const bool tx_enable = true) {
    const vip::uart::UartParams params =
        tc_uart_params_for(BASIC_BAUD_RATE,
                           data_bits_from_cfg(data_mode),
                           stop_bits_from_cfg(stop_mode),
                           parity_from_cfg(parity_mode));
    const UartCoreConfig cfg =
        tc_make_uart_config(BASIC_BAUD_RATE,
                            parity_mode,
                            stop_mode,
                            data_mode,
                            true,
                            rx_enable,
                            tx_enable,
                            false);
    co_await tc_apply_uart_config(test, cfg, params);
    co_await wait_cycles(test, 2u);
    co_return;
}

TestBase::RunUserTask wait_status(Test& test,
                                  std::function<bool(const UartCoreStatus&)> pred,
                                  std::string label,
                                  UartCoreStatus* final_status = nullptr,
                                  const unsigned timeout_cycles =
                                      STRESS_STATUS_TIMEOUT_CYCLES) {
    UartCoreStatus status{};
    for (unsigned cycle = 0u; cycle < timeout_cycles; ++cycle) {
        co_await test.core_intf.sample_status(status);
        if (pred(status)) {
            if (final_status != nullptr) {
                *final_status = status;
            }
            co_return;
        }
        co_await test.utils.clock(1, 1);
    }

    test.scb.note_fail("tc_stress_no_cts: " + label + ": status wait timed out");
    if (final_status != nullptr) {
        *final_status = status;
    }
    co_return;
}

TestBase::RunUserTask wait_core_idle(Test& test, std::string label) {
    co_await wait_status(test,
                         [](const UartCoreStatus& status) {
                             return status.rx_empty
                                 && status.tx_empty
                                 && !status.rx_busy
                                 && !status.tx_busy
                                 && status.rx_level == 0u
                                 && status.tx_level == 0u;
                         },
                         label);
    co_return;
}

TestBase::RunUserTask wait_rx_level(Test& test,
                                    const unsigned level,
                                    std::string label) {
    co_await wait_status(test,
                         [level](const UartCoreStatus& status) {
                             return status.rx_level == level
                                 && status.rx_empty == (level == 0u)
                                 && status.rx_byte_valid == (level != 0u);
                         },
                         label);
    co_return;
}

TestBase::RunUserTask wait_tx_level(Test& test,
                                    const unsigned level,
                                    std::string label) {
    co_await wait_status(test,
                         [level](const UartCoreStatus& status) {
                             return status.tx_level == level
                                 && status.tx_empty == (level == 0u)
                                 && status.tx_byte_ready == (level < TX_FIFO_DEPTH);
                         },
                         label);
    co_return;
}

void expect_rx_events_unchanged(Test& test,
                                const UartCoreEventCounts& before,
                                const std::string& label) {
    const UartCoreEventCounts after = test.core_intf.event_counts();
    check_true(test, after.rx_overrun == before.rx_overrun, label + ": event_rx_overrun changed");
    check_true(test,
               after.rx_frame_error == before.rx_frame_error,
               label + ": event_rx_frame_error changed");
    check_true(test,
               after.rx_parity_error == before.rx_parity_error,
               label + ": event_rx_parity_error changed");
    check_true(test,
               after.rx_break_detect == before.rx_break_detect,
               label + ": event_rx_break_detect changed");
}

TestBase::RunUserTask wait_tx_done_delta(Test& test,
                                         const UartCoreEventCounts& before,
                                         const std::uint64_t delta,
                                         std::string label,
                                         const unsigned timeout_cycles =
                                             STRESS_STATUS_TIMEOUT_CYCLES * 4u) {
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

TestBase::RunUserTask send_rx_bytes(Test& test,
                                    const std::vector<std::uint8_t>& data,
                                    const vip::uart::UartParams& params) {
    const unsigned ticket = test.uart_peer_tx.enqueue_bytes(uart_rx_port_name, data);
    co_await test.uart_peer_tx.wait_done(ticket);
    co_await wait_cycles(test, params.bit_clks * 4u);
    co_return;
}

TestBase::RunUserTask pop_rx_expect(Test& test,
                                    const std::uint8_t data,
                                    const vip::uart::UartParams& params,
                                    std::string label) {
    const std::uint8_t expected = static_cast<std::uint8_t>(data & data_mask(params.data_bits));
    test.scb_core.expect_rx_byte(make_expected_rx_byte(expected));

    UartCoreRxByte observed{};
    co_await test.core_intf.pop_rx_byte(observed, HANDSHAKE_TIMEOUT_CYCLES * 4u);
    if (observed.valid) {
        check_true(test, observed.data == expected, label + ": RX data mismatch");
        check_true(test, !observed.frame_error, label + ": unexpected frame_error");
        check_true(test, !observed.parity_error, label + ": unexpected parity_error");
        check_true(test, !observed.break_detect, label + ": unexpected break_detect");
    }
    co_return;
}

void expect_tx_bytes(Test& test,
                     const std::vector<std::uint8_t>& data,
                     const vip::uart::UartParams& params) {
    for (const std::uint8_t byte : data) {
        vip::uart::UartFrame frame{};
        frame.data = static_cast<std::uint8_t>(byte & data_mask(params.data_bits));
        frame.data_bits = params.data_bits;
        frame.stop_bits = params.stop_bits;
        frame.parity = params.parity;
        test.scb_uart_stream.expect_frame(uart_tx_port_name, frame);
        test.scb_core.expect_tx_byte(frame.data);
    }
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

    test.scb.note_fail("tc_stress_no_cts: " + label + ": UART TX frame wait timed out");
    co_return;
}

void observe_tx_frames(Test& test,
                       std::size_t& next_index,
                       const std::size_t expected_total,
                       const std::string& label) {
    const auto history = test.uart_peer_rx.get_history(uart_tx_port_name);
    if (history.size() < expected_total) {
        test.scb.note_fail("tc_stress_no_cts: " + label + ": missing UART TX frames");
    }
    if (history.size() > expected_total) {
        test.scb.note_fail("tc_stress_no_cts: " + label + ": extra UART TX frames observed");
    }

    const std::size_t stop = std::min(history.size(), expected_total);
    for (std::size_t i = next_index; i < stop; ++i) {
        test.scb_core.observe_uart_tx_frame(history.at(i));
    }
    next_index = stop;
}

TestBase::RunUserTask expect_idle_status(Test& test, std::string label) {
    UartCoreStatus status{};
    co_await wait_status(test,
                         [](const UartCoreStatus& s) {
                             return s.rx_empty
                                 && s.tx_empty
                                 && s.rx_level == 0u
                                 && s.tx_level == 0u
                                 && !s.rx_full
                                 && !s.tx_full
                                 && !s.rx_busy
                                 && !s.tx_busy;
                         },
                         label,
                         &status);
    test.scb_core.check_idle_status(status, "tc_stress_no_cts " + label);
    co_return;
}

TestBase::RunUserTask push_tx_bytes_with_gaps(Test& test,
                                              const std::vector<std::uint8_t>& data,
                                              const std::vector<unsigned>& gaps) {
    for (std::size_t i = 0u; i < data.size(); ++i) {
        co_await test.core_intf.push_tx_byte(data.at(i));
        const unsigned gap = i < gaps.size() ? gaps.at(i) : 0u;
        co_await wait_cycles(test, gap);
    }
    co_return;
}

TestBase::RunUserTask subcase_simultaneous_rx_tx_small_burst(Test& test) {
    log_subcase("simultaneous RX/TX small burst");
    co_await tc_local_reset(test);
    co_await apply_stress_config(test);
    co_await test.core_intf.set_rx_ready(false);

    const vip::uart::UartParams params = make_uart_params();
    const std::vector<std::uint8_t> tx_data = {0x10u, 0x21u, 0x32u, 0x43u, 0x54u, 0x65u};
    const std::vector<std::uint8_t> rx_data = {0xa0u, 0xb1u, 0xc2u, 0xd3u, 0xe4u, 0xf5u};
    const UartCoreEventCounts before = test.core_intf.event_counts();
    test.uart_peer_rx.clear_history(uart_tx_port_name);
    std::size_t observed_tx = 0u;

    expect_tx_bytes(test, tx_data, params);
    const unsigned rx_ticket = test.uart_peer_tx.enqueue_bytes(uart_rx_port_name, rx_data);
    co_await push_tx_bytes_with_gaps(test, tx_data, {0u, 2u, 0u, 3u, 1u, 0u});
    co_await test.uart_peer_tx.wait_done(rx_ticket);
    co_await wait_cycles(test, params.bit_clks * 4u);

    co_await wait_rx_level(test, static_cast<unsigned>(rx_data.size()), "simultaneous RX fill");
    co_await wait_tx_frame_count(test, tx_data.size(), tx_timeout_for(params, tx_data.size()), "simultaneous TX");
    observe_tx_frames(test, observed_tx, tx_data.size(), "simultaneous TX");

    for (const std::uint8_t data : rx_data) {
        co_await pop_rx_expect(test, data, params, "simultaneous RX pop");
    }
    co_await wait_tx_done_delta(test, before, tx_data.size(), "simultaneous TX done");
    expect_rx_events_unchanged(test, before, "simultaneous RX/TX");
    co_await expect_idle_status(test, "simultaneous final idle");
    co_return;
}

TestBase::RunUserTask subcase_rx_backpressure_delayed_popping(Test& test) {
    log_subcase("RX backpressure with delayed popping");
    co_await tc_local_reset(test);
    co_await apply_stress_config(test);
    co_await test.core_intf.set_rx_ready(false);

    const vip::uart::UartParams params = make_uart_params();
    const std::vector<std::uint8_t> rx_data = {0x30u, 0x31u, 0x32u, 0x33u, 0x34u, 0x35u, 0x36u, 0x37u};
    const UartCoreEventCounts before = test.core_intf.event_counts();

    co_await send_rx_bytes(test, rx_data, params);
    UartCoreStatus status{};
    co_await wait_status(test,
                         [](const UartCoreStatus& s) {
                             return s.rx_level == 8u
                                 && !s.rx_empty
                                 && !s.rx_full
                                 && s.rx_byte_valid;
                         },
                         "RX backpressure queued level",
                         &status);

    co_await pop_rx_expect(test, rx_data.at(0), params, "RX backpressure pop 0");
    co_await wait_cycles(test, 2u);
    co_await pop_rx_expect(test, rx_data.at(1), params, "RX backpressure pop 1");
    co_await wait_cycles(test, 5u);
    co_await pop_rx_expect(test, rx_data.at(2), params, "RX backpressure pop 2");
    co_await pop_rx_expect(test, rx_data.at(3), params, "RX backpressure pop 3");
    co_await wait_cycles(test, 1u);
    for (std::size_t i = 4u; i < rx_data.size(); ++i) {
        co_await pop_rx_expect(test, rx_data.at(i), params, "RX backpressure remaining pop");
    }

    co_await wait_rx_level(test, 0u, "RX backpressure final empty");
    expect_rx_events_unchanged(test, before, "RX backpressure");
    co_return;
}

TestBase::RunUserTask subcase_tx_gaps_pause_resume(Test& test) {
    log_subcase("TX valid gaps and TX enable pause/resume");
    co_await tc_local_reset(test);
    co_await apply_stress_config(test);

    const vip::uart::UartParams params = make_uart_params();
    const std::vector<std::uint8_t> first = {0x81u, 0x82u, 0x83u};
    const std::vector<std::uint8_t> second = {0x91u, 0x92u, 0x93u};
    const std::vector<std::uint8_t> all = {0x81u, 0x82u, 0x83u, 0x91u, 0x92u, 0x93u};
    const UartCoreEventCounts before = test.core_intf.event_counts();
    test.uart_peer_rx.clear_history(uart_tx_port_name);
    std::size_t observed_tx = 0u;

    expect_tx_bytes(test, all, params);
    co_await push_tx_bytes_with_gaps(test, first, {3u, 7u, 2u});
    co_await apply_stress_config(test, UART_PARITY_NONE, UART_STOP_1, UART_DATA_8, true, false);
    co_await push_tx_bytes_with_gaps(test, second, {4u, 1u, 0u});
    co_await wait_cycles(test, params.frame_clks() * 2u);

    UartCoreStatus disabled_status{};
    co_await wait_status(test,
                         [](const UartCoreStatus& s) {
                             return !s.tx_busy && !s.tx_empty && s.tx_level != 0u;
                         },
                         "TX pause disabled queued state",
                         &disabled_status,
                         params.frame_clks() * 3u);

    co_await apply_stress_config(test);
    co_await wait_tx_frame_count(test, all.size(), tx_timeout_for(params, all.size()), "TX pause/resume");
    observe_tx_frames(test, observed_tx, all.size(), "TX pause/resume");
    co_await wait_tx_done_delta(test, before, all.size(), "TX pause/resume done");
    co_await expect_idle_status(test, "TX pause/resume final idle");
    co_return;
}

struct FormatCase {
    const char* name;
    unsigned parity_mode;
    unsigned stop_mode;
    unsigned data_mode;
    std::uint8_t data;
};

TestBase::RunUserTask subcase_mixed_legal_formats(Test& test) {
    log_subcase("mixed legal formats while idle");
    co_await tc_local_reset(test);

    const std::vector<FormatCase> cases = {
        {"8N1", UART_PARITY_NONE, UART_STOP_1, UART_DATA_8, 0x55u},
        {"7E1", UART_PARITY_EVEN, UART_STOP_1, UART_DATA_7, 0x35u},
        {"8O2", UART_PARITY_ODD, UART_STOP_2, UART_DATA_8, 0xa6u},
        {"5N1", UART_PARITY_NONE, UART_STOP_1, UART_DATA_5, 0x1bu},
    };

    for (const FormatCase& c : cases) {
        co_await wait_core_idle(test, std::string(c.name) + " pre-config idle");
        co_await apply_stress_config(test, c.parity_mode, c.stop_mode, c.data_mode);
        co_await test.core_intf.set_rx_ready(false);

        const vip::uart::UartParams params =
            tc_uart_params_for(BASIC_BAUD_RATE,
                               data_bits_from_cfg(c.data_mode),
                               stop_bits_from_cfg(c.stop_mode),
                               parity_from_cfg(c.parity_mode));
        const UartCoreEventCounts before = test.core_intf.event_counts();

        co_await send_rx_byte(test, c.data, params);
        co_await pop_rx_expect(test, c.data, params, std::string(c.name) + " RX");
        expect_rx_events_unchanged(test, before, std::string(c.name) + " RX");

        test.uart_peer_rx.clear_history(uart_tx_port_name);
        std::size_t observed_tx = 0u;
        const UartCoreEventCounts before_tx = test.core_intf.event_counts();
        expect_tx_bytes(test, {c.data}, params);
        co_await test.core_intf.push_tx_byte(c.data);
        co_await wait_tx_frame_count(test, 1u, tx_timeout_for(params, 1u), std::string(c.name) + " TX");
        observe_tx_frames(test, observed_tx, 1u, std::string(c.name) + " TX");
        co_await wait_tx_done_delta(test, before_tx, 1u, std::string(c.name) + " TX done");
        co_await wait_core_idle(test, std::string(c.name) + " final idle");
    }
    co_return;
}

TestBase::RunUserTask subcase_near_fifo_pressure_parallel(Test& test) {
    log_subcase("near-FIFO-pressure parallel traffic");
    co_await tc_local_reset(test);
    co_await apply_stress_config(test, UART_PARITY_NONE, UART_STOP_1, UART_DATA_8, true, false);
    co_await test.core_intf.set_rx_ready(false);

    const vip::uart::UartParams params = make_uart_params();
    const std::vector<std::uint8_t> tx_data = {0xc0u, 0xc1u, 0xc2u, 0xc3u, 0xc4u, 0xc5u, 0xc6u, 0xc7u};
    const std::vector<std::uint8_t> rx_data = {0xd0u, 0xd1u, 0xd2u, 0xd3u, 0xd4u, 0xd5u, 0xd6u, 0xd7u};
    const UartCoreEventCounts before = test.core_intf.event_counts();
    test.uart_peer_rx.clear_history(uart_tx_port_name);
    std::size_t observed_tx = 0u;

    for (const std::uint8_t data : tx_data) {
        co_await test.core_intf.push_tx_byte(data);
    }
    co_await wait_tx_level(test, static_cast<unsigned>(tx_data.size()), "near pressure TX queued");
    co_await send_rx_bytes(test, rx_data, params);

    UartCoreStatus status{};
    co_await wait_status(test,
                         [](const UartCoreStatus& s) {
                             return s.tx_level == 8u
                                 && s.rx_level == 8u
                                 && !s.tx_full
                                 && !s.rx_full;
                         },
                         "near pressure queued levels",
                         &status);

    expect_tx_bytes(test, tx_data, params);
    co_await apply_stress_config(test);

    for (std::size_t i = 0u; i < rx_data.size(); ++i) {
        co_await pop_rx_expect(test, rx_data.at(i), params, "near pressure RX pop");
        if ((i % 2u) == 0u) {
            co_await wait_cycles(test, 3u);
        }
    }

    co_await wait_tx_frame_count(test, tx_data.size(), tx_timeout_for(params, tx_data.size()), "near pressure TX");
    observe_tx_frames(test, observed_tx, tx_data.size(), "near pressure TX");
    co_await wait_tx_done_delta(test, before, tx_data.size(), "near pressure TX done");
    expect_rx_events_unchanged(test, before, "near pressure RX");
    co_await expect_idle_status(test, "near pressure final idle");
    co_return;
}

} // namespace

TestBase::RunUserTask tc_stress_no_cts(Test& test) {

    co_await subcase_simultaneous_rx_tx_small_burst(test);
    co_await subcase_rx_backpressure_delayed_popping(test);
    co_await subcase_tx_gaps_pause_resume(test);
    co_await subcase_mixed_legal_formats(test);
    co_await subcase_near_fifo_pressure_parallel(test);

    if (!test.scb.case_has_failures()) {
        test.scb.note_pass("tc_stress_no_cts mixed no-CTS stress completed");
    }

    co_return;
}

} // namespace test

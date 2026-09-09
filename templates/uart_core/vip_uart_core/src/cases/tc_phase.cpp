#include "tc_phase.hpp"

// DUT parameter requirements for this testcase:
//   No tc_phase-specific DUT generic overrides are required.
// The testcase uses the normal 100 MHz / 921600 baud / 8N1 configuration and
// drives sub-clock UART start-edge phases through the reusable vip_uart TX agent.

#include <cstddef>
#include <cstdint>
#include <functional>
#include <iomanip>
#include <sstream>
#include <string>
#include <vector>

#include "tc_utils.hpp"

#include "../pindefs.hpp"

#include "vip_common/common/logger.hpp"

namespace test {
namespace {

static constexpr std::uint64_t PHASE_BAUD_RATE = BASIC_BAUD_RATE;
static constexpr unsigned PHASE_STATUS_TIMEOUT_CYCLES = BASIC_UART_BIT_CLKS * 64u;

void log_subcase(const std::string& name) {
    vip::common::log_line("tc_phase", "INFO", "subcase " + name);
}

void check_true(Test& test, const bool condition, const std::string& msg) {
    if (!condition) {
        test.scb.note_fail("tc_phase: " + msg);
    }
}

std::string hex_byte(const std::uint8_t data) {
    std::ostringstream oss;
    oss << "0x" << std::hex << std::uppercase << std::setw(2)
        << std::setfill('0') << static_cast<unsigned>(data);
    return oss.str();
}

std::string phase_context(const std::uint64_t phase_offset_ps,
                          const std::uint8_t data,
                          const std::string& label) {
    return label + ": phase_offset_ps=" + std::to_string(phase_offset_ps)
        + " data=" + hex_byte(data);
}

TestBase::RunUserTask wait_cycles(Test& test, const unsigned cycles) {
    if (cycles != 0u) {
        co_await test.utils.clock(static_cast<int>(cycles), 1);
    }
    co_return;
}

TestBase::RunUserTask apply_phase_config(Test& test) {
    co_await tc_apply_basic_config(test);
    co_await wait_cycles(test, 2u);
    co_await test.core_intf.set_rx_ready(false);
    co_return;
}

void expect_no_rx_event_delta(Test& test,
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

TestBase::RunUserTask wait_rx_status(Test& test,
                                     std::function<bool(const UartCoreStatus&)> pred,
                                     std::string label) {
    UartCoreStatus status{};
    for (unsigned cycle = 0u; cycle < PHASE_STATUS_TIMEOUT_CYCLES; ++cycle) {
        co_await test.core_intf.sample_status(status);
        if (pred(status)) {
            co_return;
        }
        co_await test.utils.clock(1, 1);
    }

    test.scb.note_fail("tc_phase: " + label + ": RX status wait timed out");
    co_return;
}

TestBase::RunUserTask wait_rx_level(Test& test,
                                    const unsigned level,
                                    std::string label) {
    co_await wait_rx_status(test,
                            [level](const UartCoreStatus& status) {
                                return status.rx_level == level
                                    && status.rx_empty == (level == 0u)
                                    && status.rx_byte_valid == (level != 0u);
                            },
                            label);
    co_return;
}

TestBase::RunUserTask wait_rx_empty_idle(Test& test, std::string label) {
    co_await wait_rx_status(test,
                            [](const UartCoreStatus& status) {
                                return status.rx_level == 0u
                                    && status.rx_empty
                                    && !status.rx_full
                                    && !status.rx_byte_valid
                                    && !status.rx_busy;
                            },
                            label);
    co_return;
}

TestBase::RunUserTask send_rx_byte_with_phase(Test& test,
                                              const std::uint8_t data,
                                              const std::uint64_t phase_offset_ps,
                                              const vip::uart::UartParams& params) {
    const unsigned ticket =
        test.uart_peer_tx.enqueue_byte_with_phase(uart_rx_port_name,
                                                  data,
                                                  PHASE_BAUD_RATE,
                                                  phase_offset_ps);
    co_await test.uart_peer_tx.wait_done(ticket);
    co_await wait_cycles(test, params.bit_clks * 4u);
    co_return;
}

TestBase::RunUserTask send_rx_bytes_with_initial_phase(Test& test,
                                                       const std::vector<std::uint8_t>& data,
                                                       const std::uint64_t phase_offset_ps,
                                                       const vip::uart::UartParams& params) {
    const unsigned ticket =
        test.uart_peer_tx.enqueue_bytes_with_phase(uart_rx_port_name,
                                                   data,
                                                   PHASE_BAUD_RATE,
                                                   phase_offset_ps);
    co_await test.uart_peer_tx.wait_done(ticket);
    co_await wait_cycles(test, params.bit_clks * 4u);
    co_return;
}

TestBase::RunUserTask pop_rx_expect(Test& test,
                                    const std::uint8_t data,
                                    const std::uint64_t phase_offset_ps,
                                    std::string label) {
    const std::string ctx = phase_context(phase_offset_ps, data, label);
    test.scb_core.expect_rx_byte(make_expected_rx_byte(data));

    UartCoreRxByte observed{};
    co_await test.core_intf.pop_rx_byte(observed, HANDSHAKE_TIMEOUT_CYCLES * 4u);
    if (observed.valid) {
        check_true(test, observed.data == data, ctx + ": RX data mismatch");
        check_true(test, !observed.frame_error, ctx + ": unexpected frame_error");
        check_true(test, !observed.parity_error, ctx + ": unexpected parity_error");
        check_true(test, !observed.break_detect, ctx + ": unexpected break_detect");
    }
    co_return;
}

TestBase::RunUserTask run_phase_byte(Test& test,
                                     const std::uint8_t data,
                                     const std::uint64_t phase_offset_ps,
                                     const vip::uart::UartParams& params,
                                     std::string label) {
    const std::string ctx = phase_context(phase_offset_ps, data, label);
    const UartCoreEventCounts before = test.core_intf.event_counts();

    co_await send_rx_byte_with_phase(test, data, phase_offset_ps, params);
    co_await pop_rx_expect(test, data, phase_offset_ps, label);
    co_await wait_rx_empty_idle(test, ctx + " final empty");
    expect_no_rx_event_delta(test, before, ctx);
    co_return;
}

TestBase::RunUserTask subcase_baseline_receive(Test& test) {
    log_subcase("baseline aligned-ish receive");
    co_await tc_local_reset(test);
    co_await apply_phase_config(test);

    const vip::uart::UartParams params = make_uart_params();
    co_await run_phase_byte(test, 0x55u, 5000u, params, "baseline receive");
    co_return;
}

TestBase::RunUserTask subcase_single_byte_phase_sweep(Test& test) {
    log_subcase("single-byte phase sweep");
    co_await tc_local_reset(test);
    co_await apply_phase_config(test);

    const vip::uart::UartParams params = make_uart_params();
    const std::vector<std::uint64_t> phases = {
        100u,
        1000u,
        2500u,
        5000u,
        7500u,
        9000u,
        9900u,
    };

    for (const std::uint64_t phase : phases) {
        co_await run_phase_byte(test, 0xa5u, phase, params, "single-byte phase sweep");
    }
    co_return;
}

TestBase::RunUserTask subcase_multi_pattern_phase_sweep(Test& test) {
    log_subcase("multi-pattern phase sweep");
    co_await tc_local_reset(test);
    co_await apply_phase_config(test);

    const vip::uart::UartParams params = make_uart_params();
    const std::vector<std::uint8_t> patterns = {
        0x00u,
        0xffu,
        0x55u,
        0xaau,
        0x81u,
        0x7eu,
        0xc3u,
        0x3cu,
    };
    const std::vector<std::uint64_t> phases = {
        100u,
        2500u,
        5000u,
        7500u,
        9900u,
    };

    for (const std::uint8_t pattern : patterns) {
        for (const std::uint64_t phase : phases) {
            co_await run_phase_byte(test, pattern, phase, params, "multi-pattern phase sweep");
        }
    }
    co_return;
}

TestBase::RunUserTask subcase_back_to_back_fixed_odd_phase(Test& test) {
    log_subcase("back-to-back frames with fixed odd phase");
    co_await tc_local_reset(test);
    co_await apply_phase_config(test);

    const vip::uart::UartParams params = make_uart_params();
    const std::uint64_t phase = 9900u;
    const std::vector<std::uint8_t> data = {
        0x10u,
        0x22u,
        0x34u,
        0x46u,
        0x58u,
    };
    const UartCoreEventCounts before = test.core_intf.event_counts();

    co_await send_rx_bytes_with_initial_phase(test, data, phase, params);
    co_await wait_rx_level(test, static_cast<unsigned>(data.size()), "back-to-back RX fill");

    for (const std::uint8_t byte : data) {
        co_await pop_rx_expect(test, byte, phase, "back-to-back fixed odd phase");
    }
    co_await wait_rx_empty_idle(test, "back-to-back fixed odd phase final empty");
    expect_no_rx_event_delta(test, before, "back-to-back fixed odd phase");
    co_return;
}

TestBase::RunUserTask subcase_repeated_changing_phase(Test& test) {
    log_subcase("repeated frames with changing phase");
    co_await tc_local_reset(test);
    co_await apply_phase_config(test);

    struct PhaseByte {
        std::uint8_t data;
        std::uint64_t phase_ps;
    };

    const vip::uart::UartParams params = make_uart_params();
    const std::vector<PhaseByte> sequence = {
        {0x31u, 100u},
        {0x32u, 2500u},
        {0x33u, 5000u},
        {0x34u, 7500u},
        {0x35u, 9900u},
        {0x36u, 1000u},
        {0x37u, 9000u},
    };

    for (const PhaseByte& item : sequence) {
        co_await run_phase_byte(test,
                                item.data,
                                item.phase_ps,
                                params,
                                "repeated changing phase");
    }
    co_return;
}

TestBase::RunUserTask subcase_near_edge_recovery(Test& test) {
    log_subcase("RX recovery after near-edge phase offsets");
    co_await tc_local_reset(test);
    co_await apply_phase_config(test);

    const vip::uart::UartParams params = make_uart_params();
    const std::vector<std::uint64_t> phases = {100u, 9900u};

    for (const std::uint64_t phase : phases) {
        const UartCoreEventCounts before = test.core_intf.event_counts();
        co_await send_rx_byte_with_phase(test, 0xe1u, phase, params);
        co_await pop_rx_expect(test, 0xe1u, phase, "near-edge recovery first byte");
        co_await wait_rx_empty_idle(test, "near-edge recovery after first byte");

        co_await send_rx_byte_with_phase(test, 0x1eu, 5000u, params);
        co_await pop_rx_expect(test, 0x1eu, 5000u, "near-edge recovery safe byte");
        co_await wait_rx_empty_idle(test, "near-edge recovery final empty");
        expect_no_rx_event_delta(test, before, "near-edge recovery phase " + std::to_string(phase));
    }
    co_return;
}

} // namespace

TestBase::RunUserTask tc_phase(Test& test) {

    co_await subcase_baseline_receive(test);
    co_await subcase_single_byte_phase_sweep(test);
    co_await subcase_multi_pattern_phase_sweep(test);
    co_await subcase_back_to_back_fixed_odd_phase(test);
    co_await subcase_repeated_changing_phase(test);
    co_await subcase_near_edge_recovery(test);

    if (!test.scb.case_has_failures()) {
        test.scb.note_pass("tc_phase RX phase-offset behavior completed");
    }

    co_return;
}

} // namespace test

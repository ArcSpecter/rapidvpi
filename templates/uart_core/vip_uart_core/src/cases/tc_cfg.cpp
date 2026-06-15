#include "tc_cfg.hpp"

#include <cstddef>
#include <cstdint>
#include <string>
#include <vector>

#include "tc_utils.hpp"

#include "../pindefs.hpp"

#include "vip_common/common/logger.hpp"

namespace test {
namespace {

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

[[nodiscard]] double absd(const double value) {
    return value < 0.0 ? -value : value;
}

void log_subcase(const std::string& name) {
    vip::common::log_line("tc_cfg", "INFO", "subcase " + name);
}

void check_true(Test& test, const bool condition, const std::string& msg) {
    if (!condition) {
        test.scb.note_fail("tc_cfg: " + msg);
    }
}

void expect_rx_counts_unchanged(Test& test,
                                const UartCoreEventCounts& before,
                                const std::string& label) {
    const UartCoreEventCounts after = test.core_intf.event_counts();
    check_true(test,
               after.rx_frame_error == before.rx_frame_error,
               label + ": unexpected event_rx_frame_error");
    check_true(test,
               after.rx_parity_error == before.rx_parity_error,
               label + ": unexpected event_rx_parity_error");
}

TestBase::RunUserTask wait_cycles(Test& test, const unsigned cycles) {
    if (cycles != 0u) {
        co_await test.utils.clock(static_cast<int>(cycles), 1);
    }
    co_return;
}

TestBase::RunUserTask wait_frame_margin(Test& test,
                                        const vip::uart::UartParams& params,
                                        const unsigned frames = 1u) {
    co_await wait_cycles(test, (params.frame_ticks() * frames) + (params.bit_ticks * 4u));
    co_return;
}

TestBase::RunUserTask wait_core_idle(Test& test,
                                     std::string label,
                                     const unsigned timeout_cycles = TX_IDLE_TIMEOUT_CYCLES * 2u) {
    for (unsigned cycle = 0u; cycle < timeout_cycles; ++cycle) {
        UartCoreStatus status{};
        co_await test.core_intf.sample_status(status);
        if (status.tx_empty && !status.tx_busy && !status.rx_busy) {
            co_return;
        }
        co_await test.utils.clock(1, 1);
    }

    test.scb.note_fail("tc_cfg: " + label + ": core idle wait timed out");
    co_return;
}

TestBase::RunUserTask apply_cfg(Test& test,
                                const std::uint32_t baud_rate,
                                const unsigned parity_mode,
                                const unsigned stop_mode,
                                const unsigned data_mode,
                                const bool enable = true,
                                const bool rx_enable = true,
                                const bool tx_enable = true,
                                const bool hw_flow_enable = false) {
    const vip::uart::UartParams params =
        tc_uart_params_for(baud_rate,
                           data_bits_from_cfg(data_mode),
                           stop_bits_from_cfg(stop_mode),
                           parity_from_cfg(parity_mode));
    const UartCoreConfig cfg =
        tc_make_uart_config(baud_rate,
                            parity_mode,
                            stop_mode,
                            data_mode,
                            enable,
                            rx_enable,
                            tx_enable,
                            hw_flow_enable);
    co_await tc_apply_uart_config(test, cfg, params);
    co_await test.utils.clock(2, 1);
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

TestBase::RunUserTask send_rx_byte(Test& test,
                                   const std::uint8_t data,
                                   const vip::uart::UartParams& params) {
    const unsigned ticket = test.uart_peer_tx.enqueue_byte(uart_rx_port_name, data);
    co_await test.uart_peer_tx.wait_done(ticket);
    co_await wait_cycles(test, params.bit_ticks * 4u);
    co_return;
}

TestBase::RunUserTask wait_tx_frame(Test& test,
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

    test.scb.note_fail("tc_cfg: " + label + ": UART TX frame wait timed out");
    co_return;
}

TestBase::RunUserTask check_uart_tx_idle_high(Test& test,
                                              const unsigned cycles,
                                              std::string label) {
    for (unsigned cycle = 0u; cycle < cycles; ++cycle) {
        co_await test.utils.clock(1, 1);
        auto r = test.getCoRead(0);
        r.read(uart_tx_o);
        co_await r;

        if ((r.getNum(uart_tx_o) & 1u) == 0u) {
            test.scb.note_fail("tc_cfg: " + label + ": uart_tx_o left idle-high state");
            co_return;
        }
    }
    co_return;
}

TestBase::RunUserTask expect_tx_byte(Test& test,
                                     const std::uint8_t push_data,
                                     const std::uint8_t expected_data,
                                     const vip::uart::UartParams& params,
                                     std::string label,
                                     vip::uart::UartFrame* observed_frame = nullptr) {
    test.uart_peer_rx.clear_history(uart_tx_port_name);

    vip::uart::UartFrame exp_frame{};
    exp_frame.data = expected_data;
    exp_frame.data_bits = params.data_bits;
    exp_frame.stop_bits = params.stop_bits;
    exp_frame.parity = params.parity;
    test.scb_uart_stream.expect_frame(uart_tx_port_name, exp_frame);
    test.scb_core.expect_tx_byte(expected_data);

    co_await test.core_intf.push_tx_byte(push_data);

    bool observed = false;
    co_await wait_tx_frame(test,
                           1u,
                           params.frame_ticks() + (params.bit_ticks * 8u) + HANDSHAKE_TIMEOUT_CYCLES,
                           label,
                           observed);

    const auto history = test.uart_peer_rx.get_history(uart_tx_port_name);
    if (observed && !history.empty()) {
        const vip::uart::UartFrame frame = history.back();
        test.scb_core.observe_uart_tx_frame(frame);
        if (observed_frame != nullptr) {
            *observed_frame = frame;
        }
    }

    co_await test.core_intf.wait_tx_idle(params.frame_ticks() + HANDSHAKE_TIMEOUT_CYCLES);
    co_return;
}

TestBase::RunUserTask expect_queued_tx_after_enable(Test& test,
                                                    const std::uint8_t expected_data,
                                                    const vip::uart::UartParams& params,
                                                    std::string label) {
    test.uart_peer_rx.clear_history(uart_tx_port_name);
    test.scb_uart_stream.expect_byte(uart_tx_port_name, expected_data);
    test.scb_core.expect_tx_byte(expected_data);

    bool observed = false;
    co_await wait_tx_frame(test,
                           1u,
                           params.frame_ticks() + (params.bit_ticks * 8u) + HANDSHAKE_TIMEOUT_CYCLES,
                           label,
                           observed);

    const auto history = test.uart_peer_rx.get_history(uart_tx_port_name);
    if (observed && !history.empty()) {
        test.scb_core.observe_uart_tx_frame(history.back());
    }

    co_await test.core_intf.wait_tx_idle(params.frame_ticks() + HANDSHAKE_TIMEOUT_CYCLES);
    co_return;
}

TestBase::RunUserTask subcase_cfg_enable(Test& test) {
    log_subcase("cfg_enable gating");
    co_await tc_local_reset(test);

    const vip::uart::UartParams params = tc_uart_params_for(BASIC_BAUD_RATE);
    co_await apply_cfg(test,
                       BASIC_BAUD_RATE,
                       UART_PARITY_NONE,
                       UART_STOP_1,
                       UART_DATA_8,
                       false,
                       true,
                       true,
                       false);

    const UartCoreEventCounts before_rx = test.core_intf.event_counts();
    co_await send_rx_byte(test, 0x5au, params);
    co_await wait_frame_margin(test, params);

    UartCoreStatus status{};
    co_await test.core_intf.sample_status(status);
    check_true(test, status.rx_empty, "cfg_enable=0 accepted RX byte into FIFO");
    check_true(test, !status.rx_byte_valid, "cfg_enable=0 asserted rx_byte_valid");
    expect_rx_counts_unchanged(test, before_rx, "cfg_enable=0 ignored RX frame");

    test.uart_peer_rx.clear_history(uart_tx_port_name);
    bool accepted = false;
    co_await test.core_intf.try_push_tx_byte(0x9eu, accepted, 8u);
    if (accepted) {
        const UartCoreEventCounts before_tx = test.core_intf.event_counts();
        co_await check_uart_tx_idle_high(test,
                                         params.frame_ticks() + (params.bit_ticks * 4u),
                                         "cfg_enable=0 queued TX");
        const UartCoreEventCounts after_tx = test.core_intf.event_counts();
        check_true(test,
                   after_tx.tx_done == before_tx.tx_done,
                   "cfg_enable=0 completed a queued TX byte");
    }

    co_await apply_cfg(test,
                       BASIC_BAUD_RATE,
                       UART_PARITY_NONE,
                       UART_STOP_1,
                       UART_DATA_8,
                       true,
                       true,
                       true,
                       false);

    if (accepted) {
        co_await expect_queued_tx_after_enable(test, 0x9eu, params, "cfg_enable restore TX");
    }

    co_await send_rx_byte(test, 0xa5u, params);
    co_await expect_rx_record(test, 0xa5u, false, false, false, "cfg_enable restore RX");
    co_return;
}

TestBase::RunUserTask subcase_cfg_rx_enable(Test& test) {
    log_subcase("cfg_rx_enable gating");
    co_await tc_local_reset(test);

    const vip::uart::UartParams params = tc_uart_params_for(BASIC_BAUD_RATE);
    co_await apply_cfg(test,
                       BASIC_BAUD_RATE,
                       UART_PARITY_NONE,
                       UART_STOP_1,
                       UART_DATA_8,
                       true,
                       false,
                       true,
                       false);

    const UartCoreEventCounts before = test.core_intf.event_counts();
    co_await send_rx_byte(test, 0x33u, params);
    co_await wait_frame_margin(test, params);

    UartCoreStatus status{};
    co_await test.core_intf.sample_status(status);
    check_true(test, status.rx_empty, "cfg_rx_enable=0 accepted RX byte into FIFO");
    check_true(test, !status.rx_byte_valid, "cfg_rx_enable=0 asserted rx_byte_valid");
    check_true(test, !status.rx_busy, "cfg_rx_enable=0 left rx_busy asserted");
    expect_rx_counts_unchanged(test, before, "cfg_rx_enable=0 ignored RX frame");

    co_await apply_cfg(test,
                       BASIC_BAUD_RATE,
                       UART_PARITY_NONE,
                       UART_STOP_1,
                       UART_DATA_8,
                       true,
                       true,
                       true,
                       false);

    co_await send_rx_byte(test, 0xccu, params);
    co_await expect_rx_record(test, 0xccu, false, false, false, "cfg_rx_enable restore");
    co_return;
}

TestBase::RunUserTask subcase_cfg_tx_enable(Test& test) {
    log_subcase("cfg_tx_enable gating");
    co_await tc_local_reset(test);

    const vip::uart::UartParams params = tc_uart_params_for(BASIC_BAUD_RATE);
    co_await apply_cfg(test,
                       BASIC_BAUD_RATE,
                       UART_PARITY_NONE,
                       UART_STOP_1,
                       UART_DATA_8,
                       true,
                       true,
                       false,
                       false);

    test.uart_peer_rx.clear_history(uart_tx_port_name);
    co_await test.core_intf.push_tx_byte(0x3cu);

    UartCoreStatus status{};
    co_await test.core_intf.sample_status(status);
    check_true(test, status.tx_level != 0u, "cfg_tx_enable=0 did not retain queued TX byte");
    check_true(test, !status.tx_empty, "cfg_tx_enable=0 reported TX FIFO empty after push");

    const UartCoreEventCounts before = test.core_intf.event_counts();
    co_await check_uart_tx_idle_high(test,
                                     params.frame_ticks() + (params.bit_ticks * 4u),
                                     "cfg_tx_enable=0 queued TX");
    co_await test.core_intf.sample_status(status);
    check_true(test, !status.tx_busy, "cfg_tx_enable=0 allowed tx_busy");
    check_true(test,
               test.uart_peer_rx.observed_count(uart_tx_port_name) == 0u,
               "cfg_tx_enable=0 launched a UART TX frame");
    check_true(test,
               test.core_intf.event_counts().tx_done == before.tx_done,
               "cfg_tx_enable=0 produced event_tx_done");

    test.uart_peer_rx.clear_history(uart_tx_port_name);
    test.scb_uart_stream.expect_byte(uart_tx_port_name, 0x3cu);
    test.scb_core.expect_tx_byte(0x3cu);

    co_await apply_cfg(test,
                       BASIC_BAUD_RATE,
                       UART_PARITY_NONE,
                       UART_STOP_1,
                       UART_DATA_8,
                       true,
                       true,
                       true,
                       false);

    bool observed = false;
    co_await wait_tx_frame(test,
                           1u,
                           params.frame_ticks() + (params.bit_ticks * 8u) + HANDSHAKE_TIMEOUT_CYCLES,
                           "cfg_tx_enable restore",
                           observed);

    const auto history = test.uart_peer_rx.get_history(uart_tx_port_name);
    if (observed && !history.empty()) {
        test.scb_core.observe_uart_tx_frame(history.back());
    }
    co_await test.core_intf.wait_tx_idle(params.frame_ticks() + HANDSHAKE_TIMEOUT_CYCLES);
    co_await test.core_intf.sample_status(status);
    check_true(test, status.tx_empty && !status.tx_busy, "cfg_tx_enable restore did not drain TX");
    check_true(test,
               test.core_intf.event_counts().tx_done == before.tx_done + 1u,
               "cfg_tx_enable restore did not produce one event_tx_done");
    co_return;
}

TestBase::RunUserTask subcase_cfg_baud_inc(Test& test) {
    log_subcase("cfg_baud_inc timing");
    co_await tc_local_reset(test);

    const vip::uart::UartParams fast = tc_uart_params_for(BASIC_BAUD_RATE);
    const vip::uart::UartParams slow = tc_uart_params_for(SLOW_BAUD_RATE);

    vip::uart::UartFrame fast_frame{};
    co_await apply_cfg(test,
                       BASIC_BAUD_RATE,
                       UART_PARITY_NONE,
                       UART_STOP_1,
                       UART_DATA_8,
                       true,
                       true,
                       true,
                       false);
    co_await expect_tx_byte(test, 0x55u, 0x55u, fast, "cfg_baud_inc fast", &fast_frame);
    co_await wait_core_idle(test, "cfg_baud_inc fast idle");

    vip::uart::UartFrame slow_frame{};
    co_await apply_cfg(test,
                       SLOW_BAUD_RATE,
                       UART_PARITY_NONE,
                       UART_STOP_1,
                       UART_DATA_8,
                       true,
                       true,
                       true,
                       false);
    co_await expect_tx_byte(test, 0xa5u, 0xa5u, slow, "cfg_baud_inc slow", &slow_frame);

    const double fast_duration = fast_frame.end_time_ns - fast_frame.start_time_ns;
    const double slow_duration = slow_frame.end_time_ns - slow_frame.start_time_ns;
    const double expected_slow = fast_duration * 2.0;
    const double tolerance = expected_slow * 0.02 > 200.0 ? expected_slow * 0.02 : 200.0;
    check_true(test,
               fast_duration > 0.0 && slow_duration > 0.0,
               "cfg_baud_inc missing TX frame timing data");
    check_true(test,
               absd(slow_duration - expected_slow) <= tolerance,
               "cfg_baud_inc slow frame duration is not approximately 2x fast duration");
    co_return;
}

TestBase::RunUserTask subcase_cfg_data_bits(Test& test) {
    log_subcase("cfg_data_bits modes");
    co_await tc_local_reset(test);

    struct Mode {
        unsigned cfg_mode;
        std::uint8_t tx_data;
        std::uint8_t rx_data;
    };

    const std::vector<Mode> modes = {
        {UART_DATA_5, 0xebu, 0x1au},
        {UART_DATA_6, 0xd6u, 0x2du},
        {UART_DATA_7, 0xadu, 0x53u},
        {UART_DATA_8, 0x5au, 0xa6u},
    };

    for (const Mode& mode : modes) {
        const unsigned bits = data_bits_from_cfg(mode.cfg_mode);
        const vip::uart::UartParams params =
            tc_uart_params_for(BASIC_BAUD_RATE, bits, 1u, vip::uart::UartParity::NONE);
        co_await wait_core_idle(test, "cfg_data_bits pre-config idle");
        co_await apply_cfg(test,
                           BASIC_BAUD_RATE,
                           UART_PARITY_NONE,
                           UART_STOP_1,
                           mode.cfg_mode,
                           true,
                           true,
                           true,
                           false);

        const std::uint8_t expected_tx = static_cast<std::uint8_t>(mode.tx_data & data_mask(bits));
        co_await expect_tx_byte(test,
                                mode.tx_data,
                                expected_tx,
                                params,
                                "cfg_data_bits TX");

        const std::uint8_t expected_rx = static_cast<std::uint8_t>(mode.rx_data & data_mask(bits));
        co_await send_rx_byte(test, mode.rx_data, params);
        co_await expect_rx_record(test,
                                  expected_rx,
                                  false,
                                  false,
                                  false,
                                  "cfg_data_bits RX");
    }
    co_return;
}

TestBase::RunUserTask subcase_cfg_parity_mode(Test& test) {
    log_subcase("cfg_parity_mode modes");
    co_await tc_local_reset(test);

    const std::vector<unsigned> parity_modes = {
        UART_PARITY_NONE,
        UART_PARITY_EVEN,
        UART_PARITY_ODD,
    };

    for (const unsigned parity_mode : parity_modes) {
        const vip::uart::UartParams params =
            tc_uart_params_for(BASIC_BAUD_RATE, 8u, 1u, parity_from_cfg(parity_mode));
        co_await wait_core_idle(test, "cfg_parity_mode pre-good idle");
        co_await apply_cfg(test,
                           BASIC_BAUD_RATE,
                           parity_mode,
                           UART_STOP_1,
                           UART_DATA_8,
                           true,
                           true,
                           true,
                           false);

        const UartCoreEventCounts before = test.core_intf.event_counts();
        co_await send_rx_byte(test, 0x35u, params);
        co_await expect_rx_record(test, 0x35u, false, false, false, "cfg_parity_mode good RX");
        check_true(test,
                   test.core_intf.event_counts().rx_parity_error == before.rx_parity_error,
                   "cfg_parity_mode good RX produced parity event");
    }

    for (const unsigned parity_mode : {UART_PARITY_EVEN, UART_PARITY_ODD}) {
        const vip::uart::UartParams params =
            tc_uart_params_for(BASIC_BAUD_RATE, 8u, 1u, parity_from_cfg(parity_mode));
        co_await wait_core_idle(test, "cfg_parity_mode pre-bad idle");
        co_await apply_cfg(test,
                           BASIC_BAUD_RATE,
                           parity_mode,
                           UART_STOP_1,
                           UART_DATA_8,
                           true,
                           true,
                           true,
                           false);

        const UartCoreEventCounts before = test.core_intf.event_counts();
        test.uart_peer_tx.arm_next_parity_error(uart_rx_port_name);
        co_await send_rx_byte(test, 0x35u, params);
        co_await expect_rx_record(test, 0x35u, false, true, false, "cfg_parity_mode bad RX");
        check_true(test,
                   test.core_intf.event_counts().rx_parity_error == before.rx_parity_error + 1u,
                   "cfg_parity_mode bad RX did not increment parity event");
    }

    for (const unsigned parity_mode : {UART_PARITY_EVEN, UART_PARITY_ODD}) {
        const vip::uart::UartParams params =
            tc_uart_params_for(BASIC_BAUD_RATE, 8u, 1u, parity_from_cfg(parity_mode));
        co_await wait_core_idle(test, "cfg_parity_mode pre-TX idle");
        co_await apply_cfg(test,
                           BASIC_BAUD_RATE,
                           parity_mode,
                           UART_STOP_1,
                           UART_DATA_8,
                           true,
                           true,
                           true,
                           false);
        co_await expect_tx_byte(test, 0x5au, 0x5au, params, "cfg_parity_mode TX");
    }
    co_return;
}

TestBase::RunUserTask subcase_cfg_stop_bits(Test& test) {
    log_subcase("cfg_stop_bits modes");
    co_await tc_local_reset(test);

    const vip::uart::UartParams stop1 =
        tc_uart_params_for(BASIC_BAUD_RATE, 8u, 1u, vip::uart::UartParity::NONE);
    const vip::uart::UartParams stop2 =
        tc_uart_params_for(BASIC_BAUD_RATE, 8u, 2u, vip::uart::UartParity::NONE);

    vip::uart::UartFrame stop1_frame{};
    co_await apply_cfg(test,
                       BASIC_BAUD_RATE,
                       UART_PARITY_NONE,
                       UART_STOP_1,
                       UART_DATA_8,
                       true,
                       true,
                       true,
                       false);
    co_await expect_tx_byte(test, 0x66u, 0x66u, stop1, "cfg_stop_bits 1-stop TX", &stop1_frame);
    co_await wait_core_idle(test, "cfg_stop_bits 1-stop idle");

    vip::uart::UartFrame stop2_frame{};
    co_await apply_cfg(test,
                       BASIC_BAUD_RATE,
                       UART_PARITY_NONE,
                       UART_STOP_2,
                       UART_DATA_8,
                       true,
                       true,
                       true,
                       false);
    co_await expect_tx_byte(test, 0x99u, 0x99u, stop2, "cfg_stop_bits 2-stop TX", &stop2_frame);

    const double one_stop_duration = stop1_frame.end_time_ns - stop1_frame.start_time_ns;
    const double two_stop_duration = stop2_frame.end_time_ns - stop2_frame.start_time_ns;
    check_true(test,
               two_stop_duration > one_stop_duration + (CLK_PERIOD_NS * stop1.bit_ticks * 0.5),
               "cfg_stop_bits 2-stop frame was not longer than 1-stop frame");

    co_await wait_core_idle(test, "cfg_stop_bits pre-bad-RX idle");
    co_await apply_cfg(test,
                       BASIC_BAUD_RATE,
                       UART_PARITY_NONE,
                       UART_STOP_1,
                       UART_DATA_8,
                       true,
                       true,
                       true,
                       false);
    const UartCoreEventCounts before = test.core_intf.event_counts();
    test.uart_peer_tx.arm_next_framing_error(uart_rx_port_name);
    co_await send_rx_byte(test, 0x77u, stop1);
    co_await expect_rx_record(test, 0x77u, true, false, false, "cfg_stop_bits bad stop RX");
    check_true(test,
               test.core_intf.event_counts().rx_frame_error == before.rx_frame_error + 1u,
               "cfg_stop_bits bad stop did not increment frame-error event");
    co_return;
}

TestBase::RunUserTask subcase_cfg_hw_flow_enable(Test& test) {
    log_subcase("cfg_hw_flow_enable sanity");
    co_await tc_local_reset(test);

    const vip::uart::UartParams params = tc_uart_params_for(BASIC_BAUD_RATE);
    co_await apply_cfg(test,
                       BASIC_BAUD_RATE,
                       UART_PARITY_NONE,
                       UART_STOP_1,
                       UART_DATA_8,
                       true,
                       true,
                       true,
                       true);

    co_await test.uart_peer_rx.drive_cts_now(uart_tx_port_name, false);
    co_await expect_tx_byte(test, 0x5cu, 0x5cu, params, "cfg_hw_flow_enable no RTS/CTS TX");

    UartCoreStatus status{};
    co_await test.core_intf.sample_status(status);
    check_true(test, !status.cts_blocked, "cfg_hw_flow_enable asserted cts_blocked with HAS_RTS_CTS=0");
    check_true(test, !status.rts_active, "cfg_hw_flow_enable asserted rts_active with HAS_RTS_CTS=0");

    co_await test.uart_peer_rx.drive_cts_now(uart_tx_port_name, true);
    co_return;
}

} // namespace

TestBase::RunUserTask tc_cfg(Test& test) {
    vip::common::log_line("tc_cfg", "INFO", "start");

    co_await subcase_cfg_enable(test);
    co_await subcase_cfg_rx_enable(test);
    co_await subcase_cfg_tx_enable(test);
    co_await subcase_cfg_baud_inc(test);
    co_await subcase_cfg_data_bits(test);
    co_await subcase_cfg_parity_mode(test);
    co_await subcase_cfg_stop_bits(test);
    co_await subcase_cfg_hw_flow_enable(test);

    if (!test.scb.case_has_failures()) {
        test.scb.note_pass("tc_cfg configuration behavior completed");
    }

    vip::common::log_line("tc_cfg", "INFO", "end");
    co_return;
}

} // namespace test

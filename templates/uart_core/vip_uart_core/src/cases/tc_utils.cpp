#include "tc_utils.hpp"

#include "../pindefs.hpp"
#include "../test.hpp"

#include "vip_common/common/logger.hpp"

namespace test {

std::uint32_t tc_calc_baud_inc(const std::uint64_t baud_rate) {
    const std::uint64_t num = baud_rate * OVERSAMPLE * BAUD_INC_SCALE;
    return static_cast<std::uint32_t>((num + (CLK_HZ / 2u)) / CLK_HZ);
}

vip::uart::UartParams tc_uart_params_for(const std::uint32_t baud_rate,
                                         const unsigned data_bits,
                                         const unsigned stop_bits,
                                         const vip::uart::UartParity parity) {
    vip::uart::UartParams params = make_uart_params();
    params.data_bits = data_bits;
    params.stop_bits = stop_bits;
    params.parity = parity;
    params.bit_ticks = static_cast<unsigned>((CLK_HZ + (baud_rate / 2u)) / baud_rate);
    params.sample_tick = params.bit_ticks / 2u;
    return params;
}

UartCoreConfig tc_make_uart_config(const std::uint32_t baud_rate,
                                   const unsigned parity_mode,
                                   const unsigned stop_bits,
                                   const unsigned data_bits,
                                   const bool enable,
                                   const bool rx_enable,
                                   const bool tx_enable,
                                   const bool hw_flow_enable) {
    UartCoreConfig cfg{};
    cfg.enable = enable;
    cfg.rx_enable = rx_enable;
    cfg.tx_enable = tx_enable;
    cfg.baud_inc = tc_calc_baud_inc(baud_rate);
    cfg.parity_mode = parity_mode;
    cfg.stop_bits = stop_bits;
    cfg.data_bits = data_bits;
    cfg.hw_flow_enable = hw_flow_enable;
    return cfg;
}

TestBase::RunUserTask tc_local_reset(Test& test) {
    vip::common::log_line("tc_utils", "INFO", "local reset start");

    co_await test.clock_agent.start(CLK_PERIOD_NS);
    co_await test.core_intf.drive_idle();
    co_await test.uart_peer_rx.drive_cts_now(uart_tx_port_name, true);

    co_await test.por.assert_reset(CLK_PERIOD_NS * 4.0);
    co_await test.por.deassert_reset(0.0);
    co_await test.utils.clock(4, 1);

    co_await tc_apply_basic_config(test);
    co_await test.utils.clock(4, 1);

    vip::common::log_line("tc_utils", "INFO", "local reset done");
    co_return;
}

TestBase::RunUserTask tc_apply_basic_config(Test& test) {
    co_await tc_apply_uart_config(test, make_basic_uart_core_config(), make_uart_params());
    co_return;
}

TestBase::RunUserTask tc_apply_uart_config(Test& test,
                                           const UartCoreConfig& cfg,
                                           const vip::uart::UartParams& params) {
    test.uart_peer_tx.set_params(params);
    test.uart_peer_rx.set_params(params);
    test.scb_uart_stream.set_params(params);
    co_await test.core_intf.apply_config(cfg);
    co_return;
}

} // namespace test

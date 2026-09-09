#ifndef VIP_UART_CORE_CASES_TC_UTILS_HPP
#define VIP_UART_CORE_CASES_TC_UTILS_HPP

#include "../test.hpp"

namespace test {

std::uint32_t tc_calc_baud_inc(std::uint64_t baud_rate);
vip::uart::UartParams tc_uart_params_for(std::uint32_t baud_rate,
                                         unsigned data_bits = 8u,
                                         unsigned stop_bits = 1u,
                                         vip::uart::UartParity parity = vip::uart::UartParity::NONE);
UartCoreConfig tc_make_uart_config(std::uint32_t baud_rate = BASIC_BAUD_RATE,
                                   unsigned parity_mode = UART_PARITY_NONE,
                                   unsigned stop_bits = UART_STOP_1,
                                   unsigned data_bits = UART_DATA_8,
                                   bool enable = true,
                                   bool rx_enable = true,
                                   bool tx_enable = true,
                                   bool hw_flow_enable = false);

TestBase::RunUserTask tc_local_reset(Test& test);
TestBase::RunUserTask tc_apply_basic_config(Test& test);
TestBase::RunUserTask tc_apply_uart_config(Test& test,
                                           const UartCoreConfig& cfg,
                                           const vip::uart::UartParams& params);

} // namespace test

#endif // VIP_UART_CORE_CASES_TC_UTILS_HPP

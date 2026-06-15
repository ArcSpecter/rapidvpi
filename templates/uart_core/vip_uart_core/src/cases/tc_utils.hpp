// MIT License

// Copyright (c) 2026 Rovshan Rustamov

// Permission is hereby granted, free of charge, to any person obtaining a copy
// of this software and associated documentation files (the "Software"), to deal
// in the Software without restriction, including without limitation the rights
// to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
// copies of the Software, and to permit persons to whom the Software is
// furnished to do so, subject to the following conditions:

// The above copyright notice and this permission notice shall be included in all
// copies or substantial portions of the Software.

// THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
// IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
// FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
// AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
// LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
// OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE
// SOFTWARE.

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

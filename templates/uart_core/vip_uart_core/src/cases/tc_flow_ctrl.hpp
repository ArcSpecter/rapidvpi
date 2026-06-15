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

#ifndef VIP_UART_CORE_CASES_TC_FLOW_CTRL_HPP
#define VIP_UART_CORE_CASES_TC_FLOW_CTRL_HPP

#include <initializer_list>
#include <string_view>

#include <rapidvpi/testbase/testbase.hpp>

#include "../test.hpp"
#include "vip_common/runner/case_helpers.hpp"

namespace test {

TestBase::RunUserTask tc_flow_ctrl(Test& test);

inline void register_tc_flow_ctrl(Test& test,
                                  std::initializer_list<std::string_view> tags = {},
                                  const bool enabled_by_default = true,
                                  const std::string_view name = "tc_flow_ctrl") {
    vip::common::add_case(test.runner,
                          name,
                          tags,
                          enabled_by_default,
                          [&test]() -> TestBase::RunUserTask {
                              return tc_flow_ctrl(test);
                          });
}

} // namespace test

#endif // VIP_UART_CORE_CASES_TC_FLOW_CTRL_HPP

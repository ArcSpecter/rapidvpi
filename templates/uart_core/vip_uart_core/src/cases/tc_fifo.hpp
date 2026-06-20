#ifndef VIP_UART_CORE_CASES_TC_FIFO_HPP
#define VIP_UART_CORE_CASES_TC_FIFO_HPP

#include <initializer_list>
#include <string_view>

#include <rapidvpi/testbase/testbase.hpp>

#include "../test.hpp"
#include "vip_common/runner/case_helpers.hpp"

namespace test {

TestBase::RunUserTask tc_fifo(Test& test);

inline void register_tc_fifo(Test& test,
                             std::initializer_list<std::string_view> tags = {},
                             const bool enabled_by_default = true,
                             const std::string_view name = "tc_fifo") {
    vip::common::add_case(test.runner,
                          name,
                          tags,
                          enabled_by_default,
                          [&test]() -> TestBase::RunUserTask {
                              return tc_fifo(test);
                          });
}

} // namespace test

#endif // VIP_UART_CORE_CASES_TC_FIFO_HPP

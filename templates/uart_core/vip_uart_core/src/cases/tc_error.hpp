#ifndef VIP_UART_CORE_CASES_TC_ERROR_HPP
#define VIP_UART_CORE_CASES_TC_ERROR_HPP

#include <initializer_list>
#include <string_view>

#include <rapidvpi/testbase/testbase.hpp>

#include "../test.hpp"
#include "vip_common/runner/case_helpers.hpp"

namespace test {

TestBase::RunUserTask tc_error(Test& test);

inline void register_tc_error(Test& test,
                              std::initializer_list<std::string_view> tags = {},
                              const bool enabled_by_default = true,
                              const std::string_view name = "tc_error") {
    vip::common::add_case(test.runner,
                          name,
                          tags,
                          enabled_by_default,
                          [&test]() -> TestBase::RunUserTask {
                              return tc_error(test);
                          });
}

} // namespace test

#endif // VIP_UART_CORE_CASES_TC_ERROR_HPP

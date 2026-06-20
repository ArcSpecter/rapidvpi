// vip_common/agents/por/por.hpp
#ifndef VIP_COMMON_AGENTS_POR_HPP
#define VIP_COMMON_AGENTS_POR_HPP

#include <string>

#include "vip_common/common/common.hpp"
#include "vip_common/common/logger.hpp"

namespace vip::common {

// POR/reset helper.
//
// This agent is intentionally test-driven:
// - It only writes its reset net when a testcase calls its methods.
// - No autonomous init task, so there are no hidden writers.
//
// The reset net name is provided by the project (e.g. "rst_n", "rx_rst_n", ...).
class Por {
public:
    using RunUserTask = TestBase::RunUserTask;

    // rst_net: reset net name in the DUT (default "rst_n")
    // active_low: true for rst_n style resets, false for active-high resets
    explicit Por(TestBase& tb, std::string rst_net = "rst_n", bool active_low = true);

    // Test-driven controls (cases call these).
    RunUserTask assert_reset();
    RunUserTask deassert_reset();

    template <test::TimeUnit U>
    RunUserTask assert_reset(test::delay_arg_t<U> hold) {
        co_await assert_reset();
        if (duration_to_ticks<U>(tb_, hold) != 0u) {
            co_await utils_.delay<U>(hold);
        }
        co_return;
    }

    template <test::TimeUnit U>
    RunUserTask deassert_reset(test::delay_arg_t<U> settle) {
        co_await deassert_reset();
        if (duration_to_ticks<U>(tb_, settle) != 0u) {
            co_await utils_.delay<U>(settle);
        }
        co_return;
    }

    // Pulse reset (assert for hold, then deassert).
    // Optional settle waits after deassert.
    template <test::TimeUnit U>
    RunUserTask pulse_reset(test::delay_arg_t<U> hold,
                            test::delay_arg_t<U> settle = 0) {
        co_await assert_reset<U>(hold);
        co_await deassert_reset<U>(settle);
        co_return;
    }

    const std::string& rst_net() const { return rst_net_; }
    bool active_low() const { return active_low_; }

private:
    TestBase& tb_;
    CommonUtils utils_;
    SimLogger log_;

    std::string rst_net_;
    bool active_low_ = true;

    int assert_val_ = 0;
    int deassert_val_ = 1;
};

} // namespace vip::common

#endif // VIP_COMMON_AGENTS_POR_HPP

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
    RunUserTask assert_reset(double hold_ns = 0.0);
    RunUserTask deassert_reset(double settle_ns = 0.0);

    // Pulse reset (assert for hold_ns, then deassert).
    // Optional settle_ns waits after deassert.
    RunUserTask pulse_reset(double hold_ns, double settle_ns = 0.0);

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

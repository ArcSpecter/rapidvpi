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

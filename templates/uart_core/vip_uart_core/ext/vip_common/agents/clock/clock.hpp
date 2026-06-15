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

// vip_common/agents/clock/clock.hpp
#ifndef VIP_COMMON_AGENTS_CLOCK_HPP
#define VIP_COMMON_AGENTS_CLOCK_HPP

#include <string>

#include "vip_common/common/common.hpp"
#include "vip_common/common/logger.hpp"

namespace vip::common {

// Generic free-running clock agent.
//
// - Registers a RapidVPI task (task_name) that toggles a single net (net_name)
// - Testcases control it via start()/stop()/set_period()
// - Only this agent instance should write its clock net
class Clock {
public:
    using RunTask = TestBase::RunTask;
    using RunUserTask = TestBase::RunUserTask;

    // net_name: the DUT port/net to toggle (e.g. "clk", "gtx_clk", "gmii_rx_clk")
    // task_name: unique task name registered into RapidVPI (e.g. "clk_run")
    explicit Clock(TestBase& tb, std::string net_name = "clk", std::string task_name = "clk_run");

    RunTask clk_run();

    // Test-driven controls.
    RunUserTask start(double period_ns);
    RunUserTask stop();
    RunUserTask set_period(double period_ns);

    bool is_running() const { return running_req_; }
    double period_ns() const { return period_req_ns_; }

    const std::string& net_name() const { return net_name_; }
    const std::string& task_name() const { return task_name_; }

private:
    TestBase& tb_;
    CommonUtils utils_;
    SimLogger log_;

    std::string net_name_;
    std::string task_name_;

    // Requested state (written by cases)
    bool running_req_ = false;
    double period_req_ns_ = 10.0;

    // Applied state (owned by clk_run loop)
    bool running_applied_ = false;
    double period_applied_ns_ = 10.0;

    // How often the clock task wakes while stopped (avoids busy-spin)
    static constexpr double IDLE_POLL_NS = 10.0;

    RunUserTask apply_requests_();
};

} // namespace vip::common

#endif // VIP_COMMON_AGENTS_CLOCK_HPP

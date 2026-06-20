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
// - Testcases control it via start<unit>()/stop()/set_period<unit>()
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
    template <test::TimeUnit U>
    RunUserTask start(test::delay_arg_t<U> period) {
        set_period_req_ticks_(duration_to_ticks<U>(tb_, period));
        running_req_ = true;
        co_return;
    }

    RunUserTask stop();

    template <test::TimeUnit U>
    RunUserTask set_period(test::delay_arg_t<U> period) {
        set_period_req_ticks_(duration_to_ticks<U>(tb_, period));
        co_return;
    }

    bool is_running() const { return running_req_; }
    test::sim_tick_t period_ticks() const { return period_req_ticks_; }

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
    test::sim_tick_t period_req_ticks_ = 10u;

    // Applied state (owned by clk_run loop)
    bool running_applied_ = false;
    test::sim_tick_t period_applied_ticks_ = 10u;

    // How often the clock task wakes while stopped (avoids busy-spin)
    static constexpr test::sim_tick_t IDLE_POLL_TICKS = 10u;

    void set_period_req_ticks_(test::sim_tick_t period_ticks);
    RunUserTask apply_requests_();
};

} // namespace vip::common

#endif // VIP_COMMON_AGENTS_CLOCK_HPP

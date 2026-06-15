// vip_common/agents/clock/clock.cpp
#include "vip_common/agents/clock/clock.hpp"

namespace vip::common {

Clock::Clock(TestBase& tb, std::string net_name, std::string task_name)
    : tb_(tb)
    , utils_(tb)
    , log_()
    , net_name_(std::move(net_name))
    , task_name_(std::move(task_name)) {
    tb_.registerTest(task_name_, [this]() { return this->clk_run().handle; });
}

Clock::RunUserTask Clock::start(double period_ns) {
    if (period_ns > 0.0) {
        period_req_ns_ = period_ns;
    }
    running_req_ = true;
    co_return;
}

Clock::RunUserTask Clock::stop() {
    running_req_ = false;
    co_return;
}

Clock::RunUserTask Clock::set_period(double period_ns) {
    if (period_ns > 0.0) {
        period_req_ns_ = period_ns;
    }
    co_return;
}

Clock::RunUserTask Clock::apply_requests_() {
    if (period_req_ns_ > 0.0) {
        period_applied_ns_ = period_req_ns_;
    }

    if (running_applied_ != running_req_) {
        running_applied_ = running_req_;

        // When stopping, park clock low deterministically.
        if (!running_applied_) {
            auto w = tb_.getCoWrite(0);
            w.write(net_name_, 0);
            co_await w;
        }
    }

    co_return;
}

Clock::RunTask Clock::clk_run() {
    // Initialize clock low once at start.
    {
        auto w = tb_.getCoWrite(0);
        w.write(net_name_, 0);
        co_await w;
    }

    for (;;) {
        // Apply any pending start/stop/period changes.
        co_await apply_requests_();

        if (!running_applied_) {
            // Sleep while stopped.
            co_await utils_.delay_ns(IDLE_POLL_NS);
            continue;
        }

        // Running: toggle clock with 50% duty cycle.
        const double half_ns = period_applied_ns_ * 0.5;
        if (half_ns <= 0.0) {
            // Degenerate period: just idle safely.
            co_await utils_.delay_ns(IDLE_POLL_NS);
            continue;
        }

        {
            auto w = tb_.getCoWrite(0);
            w.write(net_name_, 1);
            co_await w;
        }
        co_await utils_.delay_ns(half_ns);

        // Allow stop/period change to take effect quickly.
        co_await apply_requests_();
        if (!running_applied_) {
            continue;
        }

        {
            auto w = tb_.getCoWrite(0);
            w.write(net_name_, 0);
            co_await w;
        }
        co_await utils_.delay_ns(half_ns);
    }

    co_return;
}

} // namespace vip::common

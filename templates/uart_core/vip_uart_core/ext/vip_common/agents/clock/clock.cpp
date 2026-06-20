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

Clock::RunUserTask Clock::stop() {
    running_req_ = false;
    co_return;
}

void Clock::set_period_req_ticks_(test::sim_tick_t period_ticks) {
    if (period_ticks < 2u) {
        period_ticks = 2u;
    }
    period_req_ticks_ = period_ticks;
}

Clock::RunUserTask Clock::apply_requests_() {
    period_applied_ticks_ = period_req_ticks_ < 2u ? 2u : period_req_ticks_;

    if (running_applied_ != running_req_) {
        running_applied_ = running_req_;

        // When stopping, park clock low deterministically.
        if (!running_applied_) {
            auto w = tb_.getCoWrite();
            w.write(net_name_, 0);
            co_await w;
        }
    }

    co_return;
}

Clock::RunTask Clock::clk_run() {
    // Initialize clock low once at start.
    {
        auto w = tb_.getCoWrite();
        w.write(net_name_, 0);
        co_await w;
    }

    for (;;) {
        // Apply any pending start/stop/period changes.
        co_await apply_requests_();

        if (!running_applied_) {
            // Sleep while stopped.
            co_await utils_.delay<test::ticks>(IDLE_POLL_TICKS);
            continue;
        }

        // Running: toggle clock with 50% duty cycle.
        const test::sim_tick_t high_ticks = period_applied_ticks_ / 2u;
        const test::sim_tick_t low_ticks = period_applied_ticks_ - high_ticks;
        if (high_ticks == 0u || low_ticks == 0u) {
            // Degenerate period: just idle safely.
            co_await utils_.delay<test::ticks>(IDLE_POLL_TICKS);
            continue;
        }

        {
            auto w = tb_.getCoWrite();
            w.write(net_name_, 1);
            co_await w;
        }
        co_await utils_.delay<test::ticks>(high_ticks);

        // Allow stop/period change to take effect quickly.
        co_await apply_requests_();
        if (!running_applied_) {
            continue;
        }

        {
            auto w = tb_.getCoWrite();
            w.write(net_name_, 0);
            co_await w;
        }
        co_await utils_.delay<test::ticks>(low_ticks);
    }

    co_return;
}

} // namespace vip::common

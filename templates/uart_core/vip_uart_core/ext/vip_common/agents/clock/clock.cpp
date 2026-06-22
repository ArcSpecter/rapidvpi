/*
 * MIT License
 *
 * Copyright (c) 2026 Rovshan Rustamov
 *
 * Permission is hereby granted, free of charge, to any person obtaining a copy
 * of this software and associated documentation files (the "Software"), to deal
 * in the Software without restriction, including without limitation the rights
 * to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
 * copies of the Software, and to permit persons to whom the Software is
 * furnished to do so, subject to the following conditions:
 *
 * The above copyright notice and this permission notice shall be included in all
 * copies or substantial portions of the Software.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
 * IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
 * AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
 * LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
 * OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE
 * SOFTWARE.
 */

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

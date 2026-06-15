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

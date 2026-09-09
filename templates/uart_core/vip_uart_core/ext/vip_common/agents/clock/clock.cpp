// MIT License
//
// Copyright (c) 2024 Rovshan Rustamov
//
// Permission is hereby granted, free of charge, to any person obtaining a copy
// of this software and associated documentation files (the "Software"), to deal
// in the Software without restriction, including without limitation the rights
// to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
// copies of the Software, and to permit persons to whom the Software is
// furnished to do so, subject to the following conditions:
//
// The above copyright notice and this permission notice shall be included in all
// copies or substantial portions of the Software.
//
// THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
// IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
// FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
// AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
// LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
// OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE
// SOFTWARE.

// vip_common/agents/clock/clock.cpp
#include "vip_common/agents/clock/clock.hpp"

#include <algorithm>

namespace vip::common {

Clock::Clock(TestBase& tb, std::string net_name, std::string task_name)
    : tb_(tb)
    , utils_(tb)
    , log_()
    , net_name_(std::move(net_name))
    , task_name_(std::move(task_name)) {
    tb_.registerTest(task_name_, [this]() { return this->clk_run().handle; });
}

Clock::Clock(TestBase& tb,
             std::string net_name,
             std::string task_name,
             NativeClockCfg native_cfg)
    : tb_(tb)
    , utils_(tb)
    , log_()
    , net_name_(std::move(net_name))
    , task_name_(std::move(task_name))
    , backend_(Backend::NativeHdl)
    , native_cfg_(std::move(native_cfg)) {
    tb_.registerTest(task_name_, [this]() { return this->clk_run().handle; });
}

Clock::RunUserTask Clock::stop() {
    clear_scheduled_start_();
    running_req_ = false;
    if (backend_ == Backend::NativeHdl) {
        co_await write_native_stop_();
    }
    co_return;
}

Clock::RunUserTask Clock::wait_stopped() {
    if (backend_ == Backend::NativeHdl) {
        auto rd = tb_.getCoRead();
        rd.read(native_cfg_.stopped_net);
        co_await rd;
        if ((static_cast<unsigned long long>(rd.getNum(native_cfg_.stopped_net)) & 1u) == 0u) {
            co_await tb_.getCoChange(native_cfg_.stopped_net, 1);
        }
        co_return;
    }

    while (!fully_stopped_applied_()) {
        co_await utils_.delay<test::ticks>(IDLE_POLL_TICKS);
    }
    co_return;
}

Clock::RunUserTask Clock::stop_and_wait() {
    co_await stop();
    co_await wait_stopped();
    co_return;
}

void Clock::set_period_req_ticks_(test::sim_tick_t period_ticks) {
    if (period_ticks < 2u) {
        period_ticks = 2u;
    }
    period_req_ticks_ = period_ticks;
}

void Clock::clear_scheduled_start_() {
    ++scheduled_generation_;
    scheduled_start_req_ = false;
    scheduled_first_rise_tick_ = INVALID_TICK;
}

bool Clock::arm_scheduled_start_(const test::sim_tick_t period_ticks,
                                 const test::sim_tick_t first_rise_tick) {
    if (running_applied_) {
        log_schedule_error_("scheduled start requires a stopped clock");
        return false;
    }

    if (scheduled_start_req_) {
        log_schedule_error_("a scheduled start is already pending");
        return false;
    }

    const test::sim_tick_t now = sim_time_ticks();
    if (!valid_tick(first_rise_tick) || first_rise_tick <= now) {
        log_schedule_error_("scheduled first-rise tick must be in the future");
        return false;
    }

    if (first_rise_tick - now < IDLE_POLL_TICKS) {
        log_schedule_error_("scheduled first-rise tick requires at least " +
                            std::to_string(IDLE_POLL_TICKS) + " ticks of lead time");
        return false;
    }

    set_period_req_ticks_(period_ticks);
    scheduled_first_rise_tick_ = first_rise_tick;
    scheduled_start_req_ = true;
    running_req_ = true;
    return true;
}

Clock::RunUserTask Clock::request_scheduled_start_(
    const test::sim_tick_t period_ticks,
    const test::sim_tick_t first_rise_tick) {
    if (backend_ == Backend::LegacyVpi) {
        (void)arm_scheduled_start_(period_ticks, first_rise_tick);
        co_return;
    }

    if (scheduled_start_req_) {
        log_schedule_error_("a scheduled start is already pending");
        co_return;
    }

    const test::sim_tick_t now = sim_time_ticks();
    if (!valid_tick(first_rise_tick) || first_rise_tick <= now) {
        log_schedule_error_("scheduled first-rise tick must be in the future");
        co_return;
    }
    if (first_rise_tick - now < IDLE_POLL_TICKS) {
        log_schedule_error_("scheduled first-rise tick requires at least " +
                            std::to_string(IDLE_POLL_TICKS) + " ticks of lead time");
        co_return;
    }

    auto rd = tb_.getCoRead();
    rd.read(native_cfg_.stopped_net);
    co_await rd;
    if ((static_cast<unsigned long long>(rd.getNum(native_cfg_.stopped_net)) & 1u) == 0u) {
        log_schedule_error_("scheduled start requires a physically stopped clock");
        co_return;
    }

    set_period_req_ticks_(period_ticks);
    co_await utils_.write_barrier();
    {
        auto w = tb_.getCoWrite();
        w.write(native_cfg_.period_ticks_net, period_req_ticks_);
        co_await w;
    }

    ++scheduled_generation_;
    scheduled_first_rise_tick_ = first_rise_tick;
    scheduled_start_req_ = true;
    running_req_ = true;
    running_applied_ = false;
    co_return;
}

bool Clock::fully_stopped_applied_() const {
    return !running_applied_ && parked_low_applied_;
}

void Clock::log_schedule_error_(const std::string& message) const {
    log_line("vip_common::Clock", "ERROR", net_name_ + ": " + message);
}

Clock::RunUserTask Clock::apply_requests_() {
    period_applied_ticks_ = period_req_ticks_ < 2u ? 2u : period_req_ticks_;

    if (running_applied_ != running_req_) {
        if (!running_applied_ && running_req_ && scheduled_start_req_) {
            co_return;
        }

        running_applied_ = running_req_;

        // When stopping, park clock low deterministically.
        if (!running_applied_) {
            parked_low_applied_ = false;
            auto w = tb_.getCoWrite();
            w.write(net_name_, 0);
            co_await w;
            parked_low_applied_ = true;
        } else {
            parked_low_applied_ = false;
        }
    }

    co_return;
}

Clock::RunUserTask Clock::write_native_start_() {
    co_await utils_.write_barrier();
    auto w = tb_.getCoWrite();
    w.write(native_cfg_.period_ticks_net, period_req_ticks_);
    w.write(native_cfg_.enable_net, 1u);
    co_await w;
    period_applied_ticks_ = period_req_ticks_;
    running_applied_ = true;
    co_return;
}

Clock::RunUserTask Clock::write_native_stop_() {
    co_await utils_.write_barrier();
    auto w = tb_.getCoWrite();
    w.write(native_cfg_.enable_net, 0u);
    co_await w;
    running_applied_ = false;
    co_return;
}

Clock::RunUserTask Clock::write_native_period_() {
    co_await utils_.write_barrier();
    auto w = tb_.getCoWrite();
    w.write(native_cfg_.period_ticks_net, period_req_ticks_);
    co_await w;
    period_applied_ticks_ = period_req_ticks_;
    co_return;
}

Clock::RunTask Clock::clk_run() {
    if (backend_ == Backend::NativeHdl) {
      {
        auto w = tb_.getCoWrite();
        w.write(native_cfg_.period_ticks_net, period_req_ticks_);
        w.write(native_cfg_.enable_net, 0u);
        co_await w;
      }
      running_applied_ = false;

      for (;;) {
        if (scheduled_start_req_ && running_req_) {
            const test::sim_tick_t target = scheduled_first_rise_tick_;
            const std::uint64_t generation = scheduled_generation_;
            const test::sim_tick_t now = sim_time_ticks();

            if (now > target) {
                log_schedule_error_("missed scheduled first-rise tick " +
                                    std::to_string(target));
                clear_scheduled_start_();
                running_req_ = false;
                continue;
            }

            if (now < target) {
                co_await utils_.delay<test::ticks>(target - now);
            }

            if (!scheduled_start_req_ || !running_req_ ||
                scheduled_generation_ != generation ||
                scheduled_first_rise_tick_ != target) {
                continue;
            }

            if (sim_time_ticks() != target) {
                log_schedule_error_("missed scheduled first-rise tick " +
                                    std::to_string(target));
                clear_scheduled_start_();
                running_req_ = false;
                continue;
            }

            auto w = tb_.getCoWrite();
            w.write(native_cfg_.period_ticks_net, period_req_ticks_);
            w.write(native_cfg_.enable_net, 1u);
            co_await w;
            period_applied_ticks_ = period_req_ticks_;
            running_applied_ = true;
            clear_scheduled_start_();
            continue;
        }

        if (running_req_) {
            auto rd = tb_.getCoRead();
            rd.read(native_cfg_.stopped_net);
            co_await rd;
            if ((static_cast<unsigned long long>(rd.getNum(native_cfg_.stopped_net)) & 1u) == 0u) {
                co_await tb_.getCoChange(native_cfg_.stopped_net, 1);
            } else {
                co_await utils_.delay<test::ticks>(IDLE_POLL_TICKS);
            }
            continue;
        }

        co_await utils_.delay<test::ticks>(IDLE_POLL_TICKS);
      }
      co_return;
    }

    // Initialize clock low once at start.
    {
        auto w = tb_.getCoWrite();
        w.write(net_name_, 0);
        co_await w;
        parked_low_applied_ = true;
    }

    for (;;) {
        // Apply any pending start/stop/period changes.
        co_await apply_requests_();

        if (!running_applied_) {
            if (scheduled_start_req_ && running_req_) {
                const test::sim_tick_t now = sim_time_ticks();

                if (now > scheduled_first_rise_tick_) {
                    log_schedule_error_("missed scheduled first-rise tick " +
                                        std::to_string(scheduled_first_rise_tick_));
                    clear_scheduled_start_();
                    running_req_ = false;
                    continue;
                }

                if (now == scheduled_first_rise_tick_) {
                    clear_scheduled_start_();
                    running_applied_ = true;
                    parked_low_applied_ = false;
                    continue;
                }

                const test::sim_tick_t remaining = scheduled_first_rise_tick_ - now;
                co_await utils_.delay<test::ticks>(std::min(IDLE_POLL_TICKS, remaining));
                continue;
            }

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

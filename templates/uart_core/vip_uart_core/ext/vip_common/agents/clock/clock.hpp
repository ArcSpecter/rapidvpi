// vip_common/agents/clock/clock.hpp
#ifndef VIP_COMMON_AGENTS_CLOCK_HPP
#define VIP_COMMON_AGENTS_CLOCK_HPP

#include <cstdint>
#include <string>

#include "vip_common/common/common.hpp"
#include "vip_common/common/logger.hpp"

namespace vip::common {

// Generic free-running clock controller. LegacyVpi generates the waveform
// through RapidVPI. NativeHdl controls an HDL generator without writing the
// actual clock net.
class Clock {
public:
    using RunTask = TestBase::RunTask;
    using RunUserTask = TestBase::RunUserTask;

    struct NativeClockCfg {
        std::string enable_net;
        std::string period_ticks_net;
        std::string stopped_net;
    };

    enum class Backend {
        LegacyVpi,
        NativeHdl,
    };

    // net_name: the DUT port/net to toggle (e.g. "clk", "gtx_clk", "gmii_rx_clk")
    // task_name: unique task name registered into RapidVPI (e.g. "clk_run")
    explicit Clock(TestBase& tb, std::string net_name = "clk", std::string task_name = "clk_run");
    Clock(TestBase& tb,
          std::string net_name,
          std::string task_name,
          NativeClockCfg native_cfg);

    RunTask clk_run();

    // Test-driven controls.
    template <test::TimeUnit U>
    RunUserTask start(test::delay_arg_t<U> period) {
        clear_scheduled_start_();
        set_period_req_ticks_(duration_to_ticks<U>(tb_, period));
        running_req_ = true;
        if (backend_ == Backend::NativeHdl) {
            co_await write_native_start_();
        }
        co_return;
    }

    RunUserTask stop();

    template <test::TimeUnit U>
    RunUserTask start_after(test::delay_arg_t<U> period,
                            test::delay_arg_t<U> delay) {
        const test::sim_tick_t now = sim_time_ticks();
        const test::sim_tick_t delay_ticks = duration_to_ticks<U>(tb_, delay);

        if (!valid_tick(now) || !valid_tick(delay_ticks) ||
            delay_ticks >= INVALID_TICK - now) {
            log_schedule_error_("start_after target tick overflow");
            co_return;
        }

        co_await request_scheduled_start_(duration_to_ticks<U>(tb_, period),
                                          now + delay_ticks);
        co_return;
    }

    template <test::TimeUnit U>
    RunUserTask start_at(test::delay_arg_t<U> period,
                         test::sim_tick_t first_rise_tick) {
        co_await request_scheduled_start_(duration_to_ticks<U>(tb_, period),
                                          first_rise_tick);
        co_return;
    }

    RunUserTask wait_stopped();
    RunUserTask stop_and_wait();

    template <test::TimeUnit U>
    RunUserTask set_period(test::delay_arg_t<U> period) {
        set_period_req_ticks_(duration_to_ticks<U>(tb_, period));
        if (backend_ == Backend::NativeHdl) {
            co_await write_native_period_();
        }
        co_return;
    }

    bool is_running() const { return running_req_; }
    test::sim_tick_t period_ticks() const { return period_req_ticks_; }

    const std::string& net_name() const { return net_name_; }
    const std::string& task_name() const { return task_name_; }
    Backend backend() const { return backend_; }

private:
    TestBase& tb_;
    CommonUtils utils_;
    SimLogger log_;

    std::string net_name_;
    std::string task_name_;
    Backend backend_ = Backend::LegacyVpi;
    NativeClockCfg native_cfg_{};

    // Requested state (written by cases)
    bool running_req_ = false;
    test::sim_tick_t period_req_ticks_ = 10u;

    // Applied state (owned by clk_run loop)
    bool running_applied_ = false;
    test::sim_tick_t period_applied_ticks_ = 10u;

    // Scheduled-start request state (written by cases, consumed by clk_run)
    bool scheduled_start_req_ = false;
    test::sim_tick_t scheduled_first_rise_tick_ = INVALID_TICK;
    std::uint64_t scheduled_generation_ = 0u;

    // True only after the Clock-owned write that parks the stopped net low completes.
    bool parked_low_applied_ = false;

    // How often the clock task wakes while stopped (avoids busy-spin)
    static constexpr test::sim_tick_t IDLE_POLL_TICKS = 10u;

    void set_period_req_ticks_(test::sim_tick_t period_ticks);
    void clear_scheduled_start_();
    bool arm_scheduled_start_(test::sim_tick_t period_ticks,
                              test::sim_tick_t first_rise_tick);
    RunUserTask request_scheduled_start_(test::sim_tick_t period_ticks,
                                         test::sim_tick_t first_rise_tick);
    bool fully_stopped_applied_() const;
    void log_schedule_error_(const std::string& message) const;
    RunUserTask apply_requests_();
    RunUserTask write_native_start_();
    RunUserTask write_native_stop_();
    RunUserTask write_native_period_();
};

} // namespace vip::common

#endif // VIP_COMMON_AGENTS_CLOCK_HPP

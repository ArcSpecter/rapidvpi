#include "vip_common/scoreboard/scoreboard.hpp"

#include <sstream>

namespace vip::common {

Scoreboard::Scoreboard(TestBase& tb)
    : tb_(tb) {
    // default: WARN|FAIL
    reset_all();
    set_print_warnings_only();
}

void Scoreboard::reset_all() {
    total_cases_run_ = 0;
    total_cases_passed_ = 0;
    total_cases_failed_ = 0;
    total_fail_events_ = 0;
    total_warn_events_ = 0;
    reset_case();
}

void Scoreboard::reset_case() {
    case_active_ = false;
    case_name_.clear();
    case_index_ = total_cases_run_;
    case_start_tick_ = INVALID_TICK;
    case_end_tick_ = INVALID_TICK;

    case_info_count_ = 0;
    case_pass_count_ = 0;
    case_warn_count_ = 0;
    case_fail_count_ = 0;

    case_events_.clear();
}

void Scoreboard::start_case(const std::string& case_name, sim_tick_t time_tick) {
    reset_case();
    case_active_ = true;
    case_name_ = case_name;
    case_start_tick_ = valid_tick(time_tick) ? time_tick : sim_time_ticks();
}

void Scoreboard::end_case(sim_tick_t time_tick) {
    if (!case_active_) return;

    case_end_tick_ = valid_tick(time_tick) ? time_tick : sim_time_ticks();
    case_active_ = false;

    total_cases_run_++;
    if (case_fail_count_ == 0u) total_cases_passed_++;
    else total_cases_failed_++;
}

void Scoreboard::note_info(const std::string& msg, const sim_tick_t time_tick) {
    push_event(Level::INFO, msg, time_tick);
}

void Scoreboard::note_pass(const std::string& msg, const sim_tick_t time_tick) {
    push_event(Level::PASS, msg, time_tick);
}

void Scoreboard::note_warn(const std::string& msg, const sim_tick_t time_tick) {
    push_event(Level::WARN, msg, time_tick);
}

void Scoreboard::note_fail(const std::string& msg, const sim_tick_t time_tick) {
    push_event(Level::FAIL, msg, time_tick);
}

void Scoreboard::enable_print_info(const bool en) {
    if (en) print_mask_ |= static_cast<std::uint8_t>(PRINT_INFO);
    else print_mask_ &= static_cast<std::uint8_t>(~PRINT_INFO);
}

void Scoreboard::enable_print_pass(const bool en) {
    if (en) print_mask_ |= static_cast<std::uint8_t>(PRINT_PASS);
    else print_mask_ &= static_cast<std::uint8_t>(~PRINT_PASS);
}

void Scoreboard::enable_print_warn(const bool en) {
    if (en) print_mask_ |= static_cast<std::uint8_t>(PRINT_WARN);
    else print_mask_ &= static_cast<std::uint8_t>(~PRINT_WARN);
}

void Scoreboard::enable_print_fail(const bool en) {
    if (en) print_mask_ |= static_cast<std::uint8_t>(PRINT_FAIL);
    else print_mask_ &= static_cast<std::uint8_t>(~PRINT_FAIL);
}

void Scoreboard::set_print_warnings_only() {
    print_mask_ = static_cast<std::uint8_t>(PRINT_WARN | PRINT_FAIL);
}

void Scoreboard::set_print_all() {
    print_mask_ = static_cast<std::uint8_t>(PRINT_INFO | PRINT_PASS | PRINT_WARN | PRINT_FAIL);
}

void Scoreboard::set_print_silent_except_fail() {
    print_mask_ = static_cast<std::uint8_t>(PRINT_FAIL);
}

const char* Scoreboard::level_str(const Level lvl) {
    switch (lvl) {
        case Level::INFO: return "INFO";
        case Level::PASS: return "PASS";
        case Level::WARN: return "WARN";
        case Level::FAIL: return "FAIL";
        default: return "?";
    }
}

bool Scoreboard::should_print(const Level lvl) const {
    const std::uint8_t m = print_mask_;
    switch (lvl) {
        case Level::INFO: return (m & PRINT_INFO) != 0u;
        case Level::PASS: return (m & PRINT_PASS) != 0u;
        case Level::WARN: return (m & PRINT_WARN) != 0u;
        case Level::FAIL: return (m & PRINT_FAIL) != 0u;
        default: return false;
    }
}

void Scoreboard::push_event(const Level lvl, const std::string& msg, const sim_tick_t time_tick) {
    Event e;
    e.level = lvl;
    e.time_tick = time_tick;
    e.msg = msg;
    case_events_.push_back(e);

    switch (lvl) {
        case Level::INFO: case_info_count_++; break;
        case Level::PASS: case_pass_count_++; break;
        case Level::WARN: case_warn_count_++; total_warn_events_++; break;
        case Level::FAIL: case_fail_count_++; total_fail_events_++; break;
        default: break;
    }

    if (should_print(lvl)) {
        if (valid_tick(time_tick)) {
            log_ << "[" << level_str(lvl) << "][tick=" << time_tick << "] " << msg << std::endl;
        }
        else {
            log_ << "[" << level_str(lvl) << "] " << msg << std::endl;
        }
    }
}

void Scoreboard::print_case_summary() const {
    std::ostringstream oss;
    oss << "[SCB][CASE] '" << case_name_ << "'";
    oss << " info=" << case_info_count_;
    oss << " pass=" << case_pass_count_;
    oss << " warn=" << case_warn_count_;
    oss << " fail=" << case_fail_count_;
    if (valid_tick(case_start_tick_) && valid_tick(case_end_tick_)) {
        const sim_tick_t delta = delta_ticks(case_start_tick_, case_end_tick_);
        oss << " start_tick=" << case_start_tick_;
        oss << " end_tick=" << case_end_tick_;
        oss << " delta_ticks=" << delta;
        oss << " delta_ns=" << format_ns(ticks_to_ns(tb_, delta));
    }
    log_ << oss.str() << std::endl;
}

void Scoreboard::print_total_summary() const {
    std::ostringstream oss;
    oss << "[SCB][TOTAL] cases_run=" << total_cases_run_;
    oss << " passed=" << total_cases_passed_;
    oss << " failed=" << total_cases_failed_;
    oss << " warn_events=" << total_warn_events_;
    oss << " fail_events=" << total_fail_events_;
    log_ << oss.str() << std::endl;
}

} // namespace vip::common

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

// vip_common/scoreboard/scoreboard.hpp
#ifndef VIP_COMMON_SCOREBOARD_HPP
#define VIP_COMMON_SCOREBOARD_HPP

#include <cstddef>
#include <cstdint>
#include <string>
#include <vector>

#include "vip_common/common/logger.hpp"

namespace vip::common {

class Scoreboard {
public:
    enum class Level : int {
        INFO = 0,
        PASS = 1,
        WARN = 2,
        FAIL = 3
    };

    struct Event {
        Level level = Level::INFO;
        sim_tick_t time_tick = INVALID_TICK; // optional; INVALID_TICK if unknown
        std::string msg;
    };

    // Print mask bits (independent toggles).
    // Default is WARN|FAIL to keep output clean.
    enum PrintMask : std::uint8_t {
        PRINT_INFO = 1u << 0,
        PRINT_PASS = 1u << 1,
        PRINT_WARN = 1u << 2,
        PRINT_FAIL = 1u << 3,
    };

    explicit Scoreboard(TestBase& tb);

    // Full reset of totals and current-case state.
    void reset_all();

    // Reset only the current-case state (does not touch totals).
    void reset_case();

    // Case lifecycle.
    void start_case(const std::string& case_name, sim_tick_t time_tick = INVALID_TICK);
    void end_case(sim_tick_t time_tick = INVALID_TICK);

    // Record events.
    void note_info(const std::string& msg, sim_tick_t time_tick = INVALID_TICK);
    void note_pass(const std::string& msg, sim_tick_t time_tick = INVALID_TICK);
    void note_warn(const std::string& msg, sim_tick_t time_tick = INVALID_TICK);
    void note_fail(const std::string& msg, sim_tick_t time_tick = INVALID_TICK);

    // Query per-case results.
    const std::string& current_case() const { return case_name_; }
    std::size_t case_index() const { return case_index_; }

    std::size_t case_info_count() const { return case_info_count_; }
    std::size_t case_pass_count() const { return case_pass_count_; }
    std::size_t case_warn_count() const { return case_warn_count_; }
    std::size_t case_fail_count() const { return case_fail_count_; }

    bool case_has_failures() const { return case_fail_count_ != 0; }

    // Query totals.
    std::size_t total_cases_run() const { return total_cases_run_; }
    std::size_t total_cases_passed() const { return total_cases_passed_; }
    std::size_t total_cases_failed() const { return total_cases_failed_; }

    std::size_t total_fail_events() const { return total_fail_events_; }
    std::size_t total_warn_events() const { return total_warn_events_; }

    bool total_has_failures() const { return total_fail_events_ != 0; }

    // Optional: access to per-case events (for debug/printing).
    const std::vector<Event>& case_events() const { return case_events_; }

    // Logging helpers (prints to SimLogger).
    void print_case_summary() const;
    void print_total_summary() const;

    // -----------------------------
    // Print policy control
    // -----------------------------
    void set_print_mask(std::uint8_t mask) { print_mask_ = mask; }
    std::uint8_t print_mask() const { return print_mask_; }

    void enable_print_info(bool en);
    void enable_print_pass(bool en);
    void enable_print_warn(bool en);
    void enable_print_fail(bool en);

    // Convenience presets:
    void set_print_warnings_only(); // WARN + FAIL
    void set_print_all(); // INFO + PASS + WARN + FAIL
    void set_print_silent_except_fail(); // FAIL only

private:
    TestBase& tb_;

    // Current case context
    std::string case_name_;
    std::size_t case_index_ = 0;

    bool case_active_ = false;
    sim_tick_t case_start_tick_ = INVALID_TICK;
    sim_tick_t case_end_tick_ = INVALID_TICK;

    // Per-case counters
    std::size_t case_info_count_ = 0;
    std::size_t case_pass_count_ = 0;
    std::size_t case_warn_count_ = 0;
    std::size_t case_fail_count_ = 0;

    // Totals across all ended cases
    std::size_t total_cases_run_ = 0;
    std::size_t total_cases_passed_ = 0;
    std::size_t total_cases_failed_ = 0;

    std::size_t total_fail_events_ = 0;
    std::size_t total_warn_events_ = 0;

    // Stored events for the current case (optional but useful).
    std::vector<Event> case_events_;

    // Logger
    mutable SimLogger log_;

    // Print policy (default WARN+FAIL)
    std::uint8_t print_mask_ = static_cast<std::uint8_t>(PRINT_WARN | PRINT_FAIL);

    void push_event(Level lvl, const std::string& msg, sim_tick_t time_tick);
    static const char* level_str(Level lvl);

    bool should_print(Level lvl) const;
};

} // namespace vip::common

#endif // VIP_COMMON_SCOREBOARD_HPP

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

#include "vip_uart/scoreboard/uart_scb/scb_uart_rules.hpp"

namespace vip::uart {

ScbUartRules::ScbUartRules(vip::common::Scoreboard& scb)
    : scb_(scb) {}

void ScbUartRules::reset_case() {
    rule_event_count_ = 0u;
}

void ScbUartRules::end_case_check(const bool fail_on_rule_events) {
    if (fail_on_rule_events && rule_event_count_ != 0u) {
        scb_.note_fail("uart rules: rule event count="
                       + std::to_string(rule_event_count_));
    }
}

void ScbUartRules::observe_frame(const std::string& port, const UartFrame& frame) {
    if (!enable_) {
        return;
    }

    if (frame.framing_error) {
        note_rule_("uart rules " + port + ": framing error observed", frame.end_tick);
    }
    if (frame.parity_error) {
        note_rule_("uart rules " + port + ": parity error observed", frame.end_tick);
    }
    if (break_warn_enable_ && frame.break_detect) {
        note_rule_("uart rules " + port + ": break condition observed", frame.end_tick);
    }
}

void ScbUartRules::observe_rts_blocked(const std::string& port, const unsigned waited_clks) {
    if (!enable_ || !verbose_) {
        return;
    }
    scb_.note_info("uart rules " + port + ": TX waiting for RTS active clks="
                   + std::to_string(waited_clks));
}

void ScbUartRules::observe_rts_timeout(const std::string& port, const unsigned waited_clks) {
    if (!enable_) {
        return;
    }
    note_rule_("uart rules " + port + ": RTS wait timeout clks="
               + std::to_string(waited_clks));
}

void ScbUartRules::observe_cts_drive(const std::string& port, const bool active) {
    if (!enable_ || !verbose_) {
        return;
    }
    scb_.note_info("uart rules " + port + ": drive CTS "
                   + std::string(active ? "active" : "inactive"));
}

void ScbUartRules::note_rule_(const std::string& msg, const test::sim_tick_t time_tick) {
    rule_event_count_++;
    if (warn_only_) {
        scb_.note_warn(msg, time_tick);
    } else {
        scb_.note_fail(msg, time_tick);
    }
}

} // namespace vip::uart

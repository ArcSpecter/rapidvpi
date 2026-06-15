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
        note_rule_("uart rules " + port + ": framing error observed", frame.end_time_ns);
    }
    if (frame.parity_error) {
        note_rule_("uart rules " + port + ": parity error observed", frame.end_time_ns);
    }
    if (break_warn_enable_ && frame.break_detect) {
        note_rule_("uart rules " + port + ": break condition observed", frame.end_time_ns);
    }
}

void ScbUartRules::observe_rts_blocked(const std::string& port, const unsigned waited_ticks) {
    if (!enable_ || !verbose_) {
        return;
    }
    scb_.note_info("uart rules " + port + ": TX waiting for RTS active ticks="
                   + std::to_string(waited_ticks));
}

void ScbUartRules::observe_rts_timeout(const std::string& port, const unsigned waited_ticks) {
    if (!enable_) {
        return;
    }
    note_rule_("uart rules " + port + ": RTS wait timeout ticks="
               + std::to_string(waited_ticks));
}

void ScbUartRules::observe_cts_drive(const std::string& port, const bool active) {
    if (!enable_ || !verbose_) {
        return;
    }
    scb_.note_info("uart rules " + port + ": drive CTS "
                   + std::string(active ? "active" : "inactive"));
}

void ScbUartRules::note_rule_(const std::string& msg, const double time_ns) {
    rule_event_count_++;
    if (warn_only_) {
        scb_.note_warn(msg, time_ns);
    } else {
        scb_.note_fail(msg, time_ns);
    }
}

} // namespace vip::uart

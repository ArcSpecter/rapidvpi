#ifndef VIP_UART_SCOREBOARD_UART_SCB_SCB_UART_RULES_HPP
#define VIP_UART_SCOREBOARD_UART_SCB_SCB_UART_RULES_HPP

#include <string>

#include "vip_common/scoreboard/scoreboard.hpp"
#include "vip_uart/common/uart_types.hpp"

namespace vip::uart {

class ScbUartRules {
public:
    explicit ScbUartRules(vip::common::Scoreboard& scb);

    void reset_case();
    void end_case_check(bool fail_on_rule_events = false);

    void set_enable(bool en) { enable_ = en; }
    void set_warn_only(bool en) { warn_only_ = en; }
    void set_verbose(bool en) { verbose_ = en; }
    void set_break_warn_enable(bool en) { break_warn_enable_ = en; }

    void observe_frame(const std::string& port, const UartFrame& frame);
    void observe_rts_blocked(const std::string& port, unsigned waited_ticks);
    void observe_rts_timeout(const std::string& port, unsigned waited_ticks);
    void observe_cts_drive(const std::string& port, bool active);

private:
    vip::common::Scoreboard& scb_;
    bool enable_ = true;
    bool warn_only_ = false;
    bool verbose_ = false;
    bool break_warn_enable_ = true;
    unsigned rule_event_count_ = 0;

    void note_rule_(const std::string& msg, double time_ns = -1.0);
};

} // namespace vip::uart

#endif // VIP_UART_SCOREBOARD_UART_SCB_SCB_UART_RULES_HPP

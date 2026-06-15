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

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

#ifndef VIP_UART_AGENTS_UART_RX_RX_HPP
#define VIP_UART_AGENTS_UART_RX_RX_HPP

#include <cstdint>
#include <cstddef>
#include <string>
#include <unordered_map>
#include <vector>

#include "vip_common/common/common.hpp"
#include "vip_uart/common/uart_params.hpp"
#include "vip_uart/common/uart_types.hpp"
#include "vip_uart/scoreboard/uart_scb/scb_uart_rules.hpp"
#include "vip_uart/scoreboard/uart_scb/scb_uart_stream.hpp"

namespace vip::uart {

class UartRx {
public:
    using TestBase = vip::common::TestBase;
    using RunTask = TestBase::RunTask;
    using RunUserTask = TestBase::RunUserTask;

    UartRx(TestBase& tb,
           std::string clock_net,
           std::string reset_net,
           std::vector<UartRxPortConfig> ports,
           UartParams params = UartParams{},
           bool reset_active_low = true);

    UartRx(TestBase& tb,
           std::string clock_net,
           std::string reset_net,
           UartRxPortConfig port,
           UartParams params = UartParams{},
           bool reset_active_low = true);

    void attach_scoreboards(ScbUartStream* stream, ScbUartRules* rules = nullptr);

    RunTask agent(unsigned idx);

    void reset_case();
    void set_verbose(bool en) { verbose_ = en; }
    void set_params(UartParams params);
    [[nodiscard]] const UartParams& params() const { return params_; }

    void set_capture_enable(const std::string& port, bool en);
    [[nodiscard]] std::vector<UartFrame> get_history(const std::string& port) const;
    [[nodiscard]] std::size_t history_size(const std::string& port) const;
    [[nodiscard]] std::size_t observed_count(const std::string& port) const;
    [[nodiscard]] std::size_t port_count() const { return ports_.size(); }
    void clear_history(const std::string& port);
    RunUserTask wait_for_frames(const std::string& port, std::size_t count);

    void set_cts_drive_enable(const std::string& port, bool en);
    void set_cts_active_low(const std::string& port, bool active_low);
    void set_cts_active(const std::string& port, bool active);
    RunUserTask drive_cts_now(const std::string& port, bool active);

private:
    struct PortState {
        UartRxPortConfig cfg;
        std::vector<UartFrame> history;
        bool capture_enable = true;
        bool cts_drive_enable = false;
        bool cts_active = true;
        std::size_t observed_count = 0u;
    };

    TestBase& tb_;
    vip::common::CommonUtils utils_;
    std::string clock_net_;
    std::string reset_net_;
    bool reset_active_low_ = true;
    UartParams params_;
    std::vector<PortState> ports_;
    std::unordered_map<std::string, std::size_t> port_index_;
    bool verbose_ = false;

    ScbUartStream* scb_stream_ = nullptr;
    ScbUartRules* scb_rules_ = nullptr;

    PortState& port_(const std::string& name);
    const PortState& port_(const std::string& name) const;

    RunUserTask wait_ticks_(unsigned ticks);
    RunUserTask sample_line_(PortState& port, bool& value);
    RunUserTask reset_asserted_(bool& asserted);
    RunUserTask drive_cts_(PortState& port);
    RunUserTask capture_frame_(PortState& port, UartFrame& frame);
};

} // namespace vip::uart

#endif // VIP_UART_AGENTS_UART_RX_RX_HPP

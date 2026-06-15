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

#ifndef VIP_UART_CORE_AGENTS_UART_CORE_INTF_INTF_HPP
#define VIP_UART_CORE_AGENTS_UART_CORE_INTF_INTF_HPP

#include <cstdint>
#include <string>

#include "agents/uart_core_intf/intf_types.hpp"
#include "vip_common/common/common.hpp"

namespace test {

class ScbUartCore;

class UartCoreIntf {
public:
    using TestBase = vip::common::TestBase;
    using RunTask = TestBase::RunTask;
    using RunUserTask = TestBase::RunUserTask;

    UartCoreIntf(TestBase& tb,
                 std::string clock_net,
                 std::string reset_net,
                 bool reset_active_low = true);

    void attach_scoreboard(ScbUartCore* scb) { scb_core_ = scb; }
    void reset_case();

    RunTask monitor_events();
    RunUserTask drive_idle();
    RunUserTask apply_config(const UartCoreConfig& cfg);
    RunUserTask pulse_rx_fifo_clear();
    RunUserTask pulse_tx_fifo_clear();

    RunUserTask push_tx_byte(std::uint8_t data,
                             unsigned timeout_cycles = HANDSHAKE_TIMEOUT_CYCLES);
    RunUserTask try_push_tx_byte(std::uint8_t data,
                                 bool& accepted,
                                 unsigned timeout_cycles = HANDSHAKE_TIMEOUT_CYCLES);
    RunUserTask pop_rx_byte(UartCoreRxByte& rec,
                            unsigned timeout_cycles = HANDSHAKE_TIMEOUT_CYCLES);
    RunUserTask set_rx_ready(bool ready);

    RunUserTask sample_status(UartCoreStatus& status);
    RunUserTask wait_tx_idle(unsigned timeout_cycles = TX_IDLE_TIMEOUT_CYCLES);
    [[nodiscard]] UartCoreEventCounts event_counts() const { return event_counts_; }

private:
    TestBase& tb_;
    vip::common::CommonUtils utils_;
    std::string clock_net_;
    std::string reset_net_;
    bool reset_active_low_ = true;
    ScbUartCore* scb_core_ = nullptr;
    UartCoreEventCounts event_counts_{};

    RunUserTask write_config_(const UartCoreConfig& cfg);
    RunUserTask write_tx_valid_(bool valid, std::uint8_t data);
    RunUserTask pulse_net_(const std::string& net);
};

} // namespace test

#endif // VIP_UART_CORE_AGENTS_UART_CORE_INTF_INTF_HPP

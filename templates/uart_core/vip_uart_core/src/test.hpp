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

#ifndef VIP_UART_CORE_TEST_HPP
#define VIP_UART_CORE_TEST_HPP

#include <rapidvpi/testbase/testbase.hpp>

#include <string>

#include "pindefs.hpp"

#include "agents/uart_core_intf/intf.hpp"
#include "scoreboard/scb_uart_core.hpp"

#include "vip_common/agents/clock/clock.hpp"
#include "vip_common/agents/por/por.hpp"
#include "vip_common/common/common.hpp"
#include "vip_common/runner/runner.hpp"
#include "vip_common/scoreboard/scoreboard.hpp"

#include "vip_uart/agents/uart_rx/rx.hpp"
#include "vip_uart/agents/uart_tx/tx.hpp"
#include "vip_uart/scoreboard/uart_scb/scb_uart_rules.hpp"
#include "vip_uart/scoreboard/uart_scb/scb_uart_stream.hpp"

extern "C" void userRegisterFactory();

namespace test {

namespace common = vip::common;
namespace uart = vip::uart;

using TestBase = vip::common::TestBase;

class Test : public TestBase {
private:
    std::string dutName;

public:
    Test();

    void initDutName();
    void initNets() override;

    const std::string& get_dut_name() const { return dutName; }

    common::Scoreboard scb;
    common::CommonUtils utils;
    common::Por por;
    common::Clock clock_agent;

    uart::UartParams uart_params;
    uart::ScbUartStream scb_uart_stream;
    uart::ScbUartRules scb_uart_rules;
    ScbUartCore scb_core;

    uart::UartTx uart_peer_tx;
    uart::UartRx uart_peer_rx;
    UartCoreIntf core_intf;

    common::Runner runner;
};

} // namespace test

#endif // VIP_UART_CORE_TEST_HPP

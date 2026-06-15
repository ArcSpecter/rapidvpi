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

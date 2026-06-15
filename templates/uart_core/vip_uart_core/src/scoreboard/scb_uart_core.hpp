#ifndef VIP_UART_CORE_SCOREBOARD_SCB_UART_CORE_HPP
#define VIP_UART_CORE_SCOREBOARD_SCB_UART_CORE_HPP

#include <cstdint>
#include <deque>
#include <string>
#include <vector>

#include "agents/uart_core_intf/intf_types.hpp"
#include "vip_common/scoreboard/scoreboard.hpp"
#include "vip_uart/common/uart_types.hpp"

namespace test {

class ScbUartCore {
public:
    explicit ScbUartCore(vip::common::Scoreboard& scb);

    void reset_case();
    void end_case_check(bool fail_on_outstanding = true);
    void set_verbose(bool en) { verbose_ = en; }

    void expect_rx_byte(UartCoreRxByte rec);
    void expect_rx_bytes(const std::vector<std::uint8_t>& data);
    void observe_rx_byte(const UartCoreRxByte& rec);

    void expect_tx_byte(std::uint8_t data);
    void expect_tx_bytes(const std::vector<std::uint8_t>& data);
    void observe_uart_tx_frame(const vip::uart::UartFrame& frame);

    void check_idle_status(const UartCoreStatus& status, const std::string& label);
    void note_fail(const std::string& msg);

private:
    vip::common::Scoreboard& scb_;
    bool verbose_ = false;
    std::deque<UartCoreRxByte> expected_rx_;
    std::deque<std::uint8_t> expected_tx_;

    static std::string rx_record_string_(const UartCoreRxByte& rec);
};

} // namespace test

#endif // VIP_UART_CORE_SCOREBOARD_SCB_UART_CORE_HPP

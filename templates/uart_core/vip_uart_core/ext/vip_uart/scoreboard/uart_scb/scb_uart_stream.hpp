#ifndef VIP_UART_SCOREBOARD_UART_SCB_SCB_UART_STREAM_HPP
#define VIP_UART_SCOREBOARD_UART_SCB_SCB_UART_STREAM_HPP

#include <cstdint>
#include <deque>
#include <string>
#include <unordered_map>
#include <vector>

#include "vip_common/scoreboard/scoreboard.hpp"
#include "vip_uart/common/uart_params.hpp"
#include "vip_uart/common/uart_types.hpp"

namespace vip::uart {

class ScbUartStream {
public:
    explicit ScbUartStream(vip::common::Scoreboard& scb,
                           UartParams params = UartParams{});

    void reset_case();
    void end_case_check(bool fail_on_outstanding = true);

    void set_verbose(bool en) { verbose_ = en; }
    void set_fail_on_unexpected(bool en) { fail_on_unexpected_ = en; }
    void set_strict_status_compare(bool en) { strict_status_compare_ = en; }
    void set_params(UartParams params);
    [[nodiscard]] const UartParams& params() const { return params_; }

    void expect_byte(const std::string& port, std::uint8_t data);
    void expect_bytes(const std::string& port, const std::vector<std::uint8_t>& data);
    void expect_frame(const std::string& port, UartFrame frame);

    void observe_frame(const std::string& port, UartFrame frame);

    [[nodiscard]] std::size_t observed_count(const std::string& port) const;
    [[nodiscard]] std::size_t expected_pending(const std::string& port) const;

private:
    vip::common::Scoreboard& scb_;
    UartParams params_;
    bool verbose_ = false;
    bool fail_on_unexpected_ = true;
    bool strict_status_compare_ = true;

    std::unordered_map<std::string, std::deque<UartFrame>> expected_;
    std::unordered_map<std::string, std::size_t> observed_;

    void note_mismatch_(const std::string& port,
                        const UartFrame& exp,
                        const UartFrame& obs);
    static std::string frame_string_(const UartFrame& frame);
};

} // namespace vip::uart

#endif // VIP_UART_SCOREBOARD_UART_SCB_SCB_UART_STREAM_HPP

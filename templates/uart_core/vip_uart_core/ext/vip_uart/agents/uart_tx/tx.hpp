#ifndef VIP_UART_AGENTS_UART_TX_TX_HPP
#define VIP_UART_AGENTS_UART_TX_TX_HPP

#include <cstdint>
#include <cstddef>
#include <deque>
#include <string>
#include <unordered_map>
#include <vector>

#include "vip_common/common/common.hpp"
#include "vip_uart/common/uart_params.hpp"
#include "vip_uart/common/uart_types.hpp"
#include "vip_uart/scoreboard/uart_scb/scb_uart_rules.hpp"
#include "vip_uart/scoreboard/uart_scb/scb_uart_stream.hpp"

namespace vip::uart {

class UartTx {
public:
    using TestBase = vip::common::TestBase;
    using RunTask = TestBase::RunTask;
    using RunUserTask = TestBase::RunUserTask;

    UartTx(TestBase& tb,
           std::string clock_net,
           std::string reset_net,
           std::vector<UartTxPortConfig> ports,
           UartParams params = UartParams{},
           bool reset_active_low = true);

    UartTx(TestBase& tb,
           std::string clock_net,
           std::string reset_net,
           UartTxPortConfig port,
           UartParams params = UartParams{},
           bool reset_active_low = true);

    void attach_scoreboards(ScbUartStream* stream, ScbUartRules* rules = nullptr);

    RunTask agent(unsigned idx);

    void reset_case();
    void set_verbose(bool en) { verbose_ = en; }
    void set_params(UartParams params);
    [[nodiscard]] const UartParams& params() const { return params_; }
    void set_auto_expect(bool en);
    void set_auto_expect(const std::string& port, bool en);

    unsigned enqueue_byte(const std::string& port, std::uint8_t data);
    unsigned enqueue_bytes(const std::string& port, const std::vector<std::uint8_t>& data);
    unsigned enqueue_byte_with_phase(const std::string& port,
                                     std::uint8_t data,
                                     std::uint64_t baud_rate,
                                     std::uint64_t phase_offset_ps);
    unsigned enqueue_bytes_with_phase(const std::string& port,
                                      const std::vector<std::uint8_t>& data,
                                      std::uint64_t baud_rate,
                                      std::uint64_t initial_phase_offset_ps);

    RunUserTask wait_done(unsigned ticket);
    [[nodiscard]] bool is_done(unsigned ticket) const;
    [[nodiscard]] std::size_t pending_count(const std::string& port) const;
    [[nodiscard]] std::size_t port_count() const { return ports_.size(); }

    void set_inter_frame_gap_ticks(const std::string& port, unsigned ticks);
    void set_respect_rts(const std::string& port, bool en);
    void set_rts_active_low(const std::string& port, bool active_low);
    void set_rts_wait_timeout_ticks(const std::string& port, unsigned ticks);

    void arm_next_framing_error(const std::string& port);
    void arm_next_parity_error(const std::string& port);

    [[nodiscard]] std::vector<UartFrame> get_history(const std::string& port) const;
    void clear_history(const std::string& port);

private:
    struct TxItem {
        unsigned ticket = 0u;
        UartFrame frame;
        bool force_bad_stop = false;
        bool force_bad_parity = false;
        bool use_time_delay = false;
        bool align_to_clock_phase = false;
        double bit_time_ns = 0.0;
        std::uint64_t phase_offset_ps = 0u;
    };

    struct PortState {
        UartTxPortConfig cfg;
        std::deque<TxItem> pending;
        std::vector<UartFrame> history;
        unsigned inter_frame_gap_ticks = 0u;
        unsigned rts_wait_timeout_ticks = 0u;
        bool respect_rts = false;
        bool auto_expect = false;
        bool next_bad_stop = false;
        bool next_bad_parity = false;
    };

    TestBase& tb_;
    vip::common::CommonUtils utils_;
    std::string clock_net_;
    std::string reset_net_;
    bool reset_active_low_ = true;
    UartParams params_;
    std::vector<PortState> ports_;
    std::unordered_map<std::string, std::size_t> port_index_;
    std::unordered_map<unsigned, bool> ticket_done_;
    unsigned next_ticket_ = 1u;
    bool verbose_ = false;

    ScbUartStream* scb_stream_ = nullptr;
    ScbUartRules* scb_rules_ = nullptr;

    PortState& port_(const std::string& name);
    const PortState& port_(const std::string& name) const;

    TxItem make_item_(PortState& port, std::uint8_t data);
    static double baud_to_bit_time_ns_(std::uint64_t baud_rate);

    RunUserTask drive_line_(PortState& port, bool logical_level);
    RunUserTask wait_ticks_(unsigned ticks);
    RunUserTask wait_item_bit_(const TxItem& item);
    RunUserTask read_bit_(const std::string& net, bool& value);
    RunUserTask reset_asserted_(bool& asserted);
    RunUserTask wait_rts_active_(PortState& port, bool& active);
    RunUserTask send_item_(PortState& port, TxItem item);
};

} // namespace vip::uart

#endif // VIP_UART_AGENTS_UART_TX_TX_HPP

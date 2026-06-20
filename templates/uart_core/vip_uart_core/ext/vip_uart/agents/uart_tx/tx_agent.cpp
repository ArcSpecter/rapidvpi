#include "vip_uart/agents/uart_tx/tx.hpp"

#include <stdexcept>
#include <utility>

namespace vip::uart {

UartTx::UartTx(TestBase& tb,
               std::string clock_net,
               std::string reset_net,
               std::vector<UartTxPortConfig> ports,
               UartParams params,
               const bool reset_active_low)
    : tb_(tb)
    , utils_(tb, clock_net)
    , clock_net_(std::move(clock_net))
    , reset_net_(std::move(reset_net))
    , reset_active_low_(reset_active_low)
    , params_(params) {
    if (!params_.valid()) {
        throw std::invalid_argument("vip_uart UartTx invalid UartParams");
    }

    ports_.reserve(ports.size());
    for (auto& cfg : ports) {
        if (cfg.name.empty() || cfg.tx_net.empty()) {
            throw std::invalid_argument("vip_uart UartTx port requires name and tx_net");
        }
        if (cfg.rts_net.empty()) {
            cfg.use_rts = false;
        }
        if (!cfg.use_rts) {
            cfg.rts_active_low = params_.rts_active_low;
        }

        PortState state;
        state.respect_rts = cfg.use_rts;
        state.cfg = std::move(cfg);
        const std::size_t idx = ports_.size();
        port_index_[state.cfg.name] = idx;
        ports_.push_back(std::move(state));
    }
}

UartTx::UartTx(TestBase& tb,
               std::string clock_net,
               std::string reset_net,
               UartTxPortConfig port,
               UartParams params,
               const bool reset_active_low)
    : UartTx(tb,
             std::move(clock_net),
             std::move(reset_net),
             std::vector<UartTxPortConfig>{std::move(port)},
             params,
             reset_active_low) {}

void UartTx::attach_scoreboards(ScbUartStream* stream, ScbUartRules* rules) {
    scb_stream_ = stream;
    scb_rules_ = rules;
}

void UartTx::set_params(UartParams params) {
    if (!params.valid()) {
        throw std::invalid_argument("vip_uart UartTx invalid UartParams");
    }
    params_ = params;
}

void UartTx::reset_case() {
    for (auto& port : ports_) {
        port.pending.clear();
        port.history.clear();
        port.next_bad_stop = false;
        port.next_bad_parity = false;
    }
    ticket_done_.clear();
}

void UartTx::set_auto_expect(const bool en) {
    for (auto& port : ports_) {
        port.auto_expect = en;
    }
}

void UartTx::set_auto_expect(const std::string& port, const bool en) {
    port_(port).auto_expect = en;
}

unsigned UartTx::enqueue_byte(const std::string& port_name, const std::uint8_t data) {
    auto& port = port_(port_name);
    port.pending.push_back(make_item_(port, data));
    return port.pending.back().ticket;
}

unsigned UartTx::enqueue_bytes(const std::string& port,
                               const std::vector<std::uint8_t>& data) {
    unsigned last = 0u;
    for (const std::uint8_t byte : data) {
        last = enqueue_byte(port, byte);
    }
    return last;
}

unsigned UartTx::enqueue_byte_with_phase(const std::string& port_name,
                                         const std::uint8_t data,
                                         const std::uint64_t baud_rate,
                                         const std::uint64_t phase_offset_ps) {
    auto& port = port_(port_name);
    TxItem item = make_item_(port, data);
    item.use_time_delay = true;
    item.align_to_clock_phase = true;
    item.bit_time_ns = baud_to_bit_time_ns_(baud_rate);
    item.phase_offset_ps = phase_offset_ps;

    port.pending.push_back(item);
    return item.ticket;
}

unsigned UartTx::enqueue_bytes_with_phase(const std::string& port_name,
                                          const std::vector<std::uint8_t>& data,
                                          const std::uint64_t baud_rate,
                                          const std::uint64_t initial_phase_offset_ps) {
    auto& port = port_(port_name);
    const double bit_time_ns = baud_to_bit_time_ns_(baud_rate);

    unsigned last = 0u;
    bool first = true;
    for (const std::uint8_t byte : data) {
        TxItem item = make_item_(port, byte);
        item.use_time_delay = true;
        item.align_to_clock_phase = first;
        item.bit_time_ns = bit_time_ns;
        item.phase_offset_ps = first ? initial_phase_offset_ps : 0u;
        first = false;

        last = item.ticket;
        port.pending.push_back(item);
    }

    return last;
}

UartTx::TxItem UartTx::make_item_(PortState& port, const std::uint8_t data) {
    TxItem item;
    item.ticket = next_ticket_++;
    item.frame.data = static_cast<std::uint8_t>(data & params_.data_mask());
    item.frame.data_bits = params_.data_bits;
    item.frame.stop_bits = params_.stop_bits;
    item.frame.parity = params_.parity;
    item.force_bad_stop = port.next_bad_stop;
    item.force_bad_parity = port.next_bad_parity;
    item.frame.framing_error = item.force_bad_stop;
    item.frame.parity_error = item.force_bad_parity;
    port.next_bad_stop = false;
    port.next_bad_parity = false;

    ticket_done_[item.ticket] = false;
    if (port.auto_expect && scb_stream_ != nullptr) {
        scb_stream_->expect_frame(port.cfg.name, item.frame);
    }

    return item;
}

double UartTx::baud_to_bit_time_ns_(const std::uint64_t baud_rate) {
    if (baud_rate == 0u) {
        throw std::invalid_argument("vip_uart UartTx phase send requires nonzero baud_rate");
    }
    return 1'000'000'000.0 / static_cast<double>(baud_rate);
}

UartTx::RunUserTask UartTx::wait_done(const unsigned ticket) {
    if (ticket_done_.find(ticket) == ticket_done_.end()) {
        co_return;
    }

    while (!is_done(ticket)) {
        co_await wait_clks_(params_.idle_poll_clks);
    }
    co_return;
}

bool UartTx::is_done(const unsigned ticket) const {
    const auto it = ticket_done_.find(ticket);
    return it != ticket_done_.end() && it->second;
}

std::size_t UartTx::pending_count(const std::string& port) const {
    return port_(port).pending.size();
}

void UartTx::set_inter_frame_gap_clks(const std::string& port, const unsigned clks) {
    port_(port).inter_frame_gap_clks = clks;
}

void UartTx::set_respect_rts(const std::string& port, const bool en) {
    auto& state = port_(port);
    if (en && state.cfg.rts_net.empty()) {
        throw std::invalid_argument("vip_uart UartTx port has no rts_net");
    }
    state.respect_rts = en;
    state.cfg.use_rts = en;
}

void UartTx::set_rts_active_low(const std::string& port, const bool active_low) {
    port_(port).cfg.rts_active_low = active_low;
}

void UartTx::set_rts_wait_timeout_clks(const std::string& port, const unsigned clks) {
    port_(port).rts_wait_timeout_clks = clks;
}

void UartTx::arm_next_framing_error(const std::string& port) {
    port_(port).next_bad_stop = true;
}

void UartTx::arm_next_parity_error(const std::string& port) {
    port_(port).next_bad_parity = true;
}

std::vector<UartFrame> UartTx::get_history(const std::string& port) const {
    return port_(port).history;
}

void UartTx::clear_history(const std::string& port) {
    port_(port).history.clear();
}

UartTx::PortState& UartTx::port_(const std::string& name) {
    const auto it = port_index_.find(name);
    if (it == port_index_.end()) {
        throw std::out_of_range("vip_uart UartTx unknown port: " + name);
    }
    return ports_.at(it->second);
}

const UartTx::PortState& UartTx::port_(const std::string& name) const {
    const auto it = port_index_.find(name);
    if (it == port_index_.end()) {
        throw std::out_of_range("vip_uart UartTx unknown port: " + name);
    }
    return ports_.at(it->second);
}

} // namespace vip::uart

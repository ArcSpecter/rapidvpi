#include "vip_uart/agents/uart_rx/rx.hpp"

#include <stdexcept>
#include <utility>

namespace vip::uart {

UartRx::UartRx(TestBase& tb,
               std::string clock_net,
               std::string reset_net,
               std::vector<UartRxPortConfig> ports,
               UartParams params,
               const bool reset_active_low)
    : tb_(tb)
    , utils_(tb, clock_net)
    , clock_net_(std::move(clock_net))
    , reset_net_(std::move(reset_net))
    , reset_active_low_(reset_active_low)
    , params_(params) {
    if (!params_.valid()) {
        throw std::invalid_argument("vip_uart UartRx invalid UartParams");
    }

    ports_.reserve(ports.size());
    for (auto& cfg : ports) {
        if (cfg.name.empty() || cfg.rx_net.empty()) {
            throw std::invalid_argument("vip_uart UartRx port requires name and rx_net");
        }
        if (cfg.cts_net.empty()) {
            cfg.drive_cts = false;
        }
        if (!cfg.drive_cts) {
            cfg.cts_active_low = params_.cts_active_low;
        }

        PortState state;
        state.cts_drive_enable = cfg.drive_cts;
        state.cfg = std::move(cfg);
        const std::size_t idx = ports_.size();
        port_index_[state.cfg.name] = idx;
        ports_.push_back(std::move(state));
    }
}

UartRx::UartRx(TestBase& tb,
               std::string clock_net,
               std::string reset_net,
               UartRxPortConfig port,
               UartParams params,
               const bool reset_active_low)
    : UartRx(tb,
             std::move(clock_net),
             std::move(reset_net),
             std::vector<UartRxPortConfig>{std::move(port)},
             params,
             reset_active_low) {}

void UartRx::attach_scoreboards(ScbUartStream* stream, ScbUartRules* rules) {
    scb_stream_ = stream;
    scb_rules_ = rules;
}

void UartRx::set_params(UartParams params) {
    if (!params.valid()) {
        throw std::invalid_argument("vip_uart UartRx invalid UartParams");
    }
    params_ = params;
}

void UartRx::reset_case() {
    for (auto& port : ports_) {
        port.history.clear();
        port.cts_active = true;
        port.observed_count = 0u;
    }
}

void UartRx::set_capture_enable(const std::string& port, const bool en) {
    port_(port).capture_enable = en;
}

std::vector<UartFrame> UartRx::get_history(const std::string& port) const {
    return port_(port).history;
}

std::size_t UartRx::history_size(const std::string& port) const {
    return port_(port).history.size();
}

std::size_t UartRx::observed_count(const std::string& port) const {
    return port_(port).observed_count;
}

void UartRx::clear_history(const std::string& port) {
    auto& state = port_(port);
    state.history.clear();
    state.observed_count = 0u;
}

UartRx::RunUserTask UartRx::wait_for_frames(const std::string& port, const std::size_t count) {
    while (observed_count(port) < count) {
        co_await wait_ticks_(params_.idle_poll_ticks);
    }
    co_return;
}

void UartRx::set_cts_drive_enable(const std::string& port, const bool en) {
    auto& state = port_(port);
    if (en && state.cfg.cts_net.empty()) {
        throw std::invalid_argument("vip_uart UartRx port has no cts_net");
    }
    state.cts_drive_enable = en;
    state.cfg.drive_cts = en;
}

void UartRx::set_cts_active_low(const std::string& port, const bool active_low) {
    port_(port).cfg.cts_active_low = active_low;
}

void UartRx::set_cts_active(const std::string& port, const bool active) {
    port_(port).cts_active = active;
}

UartRx::RunUserTask UartRx::drive_cts_now(const std::string& port, const bool active) {
    auto& state = port_(port);
    state.cts_active = active;
    co_await drive_cts_(state);
    co_return;
}

UartRx::PortState& UartRx::port_(const std::string& name) {
    const auto it = port_index_.find(name);
    if (it == port_index_.end()) {
        throw std::out_of_range("vip_uart UartRx unknown port: " + name);
    }
    return ports_.at(it->second);
}

const UartRx::PortState& UartRx::port_(const std::string& name) const {
    const auto it = port_index_.find(name);
    if (it == port_index_.end()) {
        throw std::out_of_range("vip_uart UartRx unknown port: " + name);
    }
    return ports_.at(it->second);
}

} // namespace vip::uart

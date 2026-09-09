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
        ++port.capture_generation;
        port.history.clear();
        port.cts_active = true;
        port.observed_count = 0u;
        port.started_count = 0u;
        port.last_start_tick = vip::common::INVALID_TICK;
        port.cts_inactive_after_count = 0u;
        port.cts_schedule_pending = false;
        port.cts_schedule_fired = false;
        port.last_driven_cts_valid = false;
        port.last_driven_cts_active = true;
        port.last_cts_transition_tick = vip::common::INVALID_TICK;
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

std::size_t UartRx::started_count(const std::string& port) const {
    return port_(port).started_count;
}

test::sim_tick_t UartRx::last_start_tick(const std::string& port) const {
    return port_(port).last_start_tick;
}

void UartRx::clear_history(const std::string& port) {
    auto& state = port_(port);
    ++state.capture_generation;
    state.history.clear();
    state.observed_count = 0u;
    state.started_count = 0u;
    state.last_start_tick = vip::common::INVALID_TICK;
}

UartRx::RunUserTask UartRx::wait_for_frames(const std::string& port, const std::size_t count) {
    while (observed_count(port) < count) {
        co_await wait_clks_(params_.idle_poll_clks);
    }
    co_return;
}

UartRx::RunUserTask UartRx::wait_for_observed_count(
    const std::string& port,
    const std::size_t count,
    const unsigned timeout_cycles,
    bool& reached) {
    reached = observed_count(port) >= count;
    for (unsigned cycle = 0u; cycle < timeout_cycles && !reached; ++cycle) {
        co_await wait_clks_(1u);
        reached = observed_count(port) >= count;
    }
    co_return;
}

UartRx::RunUserTask UartRx::wait_for_started_count(
    const std::string& port,
    const std::size_t count,
    const unsigned timeout_cycles,
    bool& reached) {
    reached = started_count(port) >= count;
    for (unsigned cycle = 0u; cycle < timeout_cycles && !reached; ++cycle) {
        co_await wait_clks_(1u);
        reached = started_count(port) >= count;
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

void UartRx::arm_cts_inactive_after_observed_count(
    const std::string& port,
    const std::size_t observed_count) {
    auto& state = port_(port);
    if (!state.cts_drive_enable || state.cfg.cts_net.empty()) {
        throw std::invalid_argument("vip_uart UartRx scheduled CTS requires an owned CTS net");
    }
    if (observed_count <= state.observed_count) {
        throw std::invalid_argument(
            "vip_uart UartRx scheduled CTS count must be in the future");
    }
    state.cts_inactive_after_count = observed_count;
    state.cts_schedule_pending = true;
    state.cts_schedule_fired = false;
}

bool UartRx::scheduled_cts_pending(const std::string& port) const {
    return port_(port).cts_schedule_pending;
}

bool UartRx::scheduled_cts_fired(const std::string& port) const {
    return port_(port).cts_schedule_fired;
}

test::sim_tick_t UartRx::last_cts_transition_tick(const std::string& port) const {
    return port_(port).last_cts_transition_tick;
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

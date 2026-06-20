#include "vip_uart/scoreboard/uart_scb/scb_uart_stream.hpp"

#include <iomanip>
#include <sstream>

namespace vip::uart {

ScbUartStream::ScbUartStream(vip::common::Scoreboard& scb, UartParams params)
    : scb_(scb), params_(params) {}

void ScbUartStream::set_params(UartParams params) {
    if (!params.valid()) {
        scb_.note_fail("uart stream: invalid UartParams");
        return;
    }
    params_ = params;
}

void ScbUartStream::reset_case() {
    expected_.clear();
    observed_.clear();
}

void ScbUartStream::end_case_check(const bool fail_on_outstanding) {
    if (!fail_on_outstanding) {
        return;
    }

    for (const auto& [port, q] : expected_) {
        if (!q.empty()) {
            scb_.note_fail("uart stream " + port + ": outstanding expected frames="
                           + std::to_string(q.size()));
        }
    }
}

void ScbUartStream::expect_byte(const std::string& port, const std::uint8_t data) {
    UartFrame frame;
    frame.data = static_cast<std::uint8_t>(data & params_.data_mask());
    frame.data_bits = params_.data_bits;
    frame.stop_bits = params_.stop_bits;
    frame.parity = params_.parity;
    expect_frame(port, frame);
}

void ScbUartStream::expect_bytes(const std::string& port,
                                 const std::vector<std::uint8_t>& data) {
    for (const std::uint8_t byte : data) {
        expect_byte(port, byte);
    }
}

void ScbUartStream::expect_frame(const std::string& port, UartFrame frame) {
    frame.data = static_cast<std::uint8_t>(frame.data & params_.data_mask());
    expected_[port].push_back(frame);
    if (verbose_) {
        scb_.note_info("uart stream " + port + ": expect " + frame_string_(frame));
    }
}

void ScbUartStream::observe_frame(const std::string& port, UartFrame frame) {
    frame.data = static_cast<std::uint8_t>(frame.data & params_.data_mask());
    observed_[port]++;

    auto& q = expected_[port];
    if (q.empty()) {
        const std::string msg = "uart stream " + port + ": unexpected observed "
                              + frame_string_(frame);
        if (fail_on_unexpected_) {
            scb_.note_fail(msg, frame.end_tick);
        } else {
            scb_.note_warn(msg, frame.end_tick);
        }
        return;
    }

    const UartFrame exp = q.front();
    q.pop_front();

    const bool data_match = (exp.data & params_.data_mask()) == (frame.data & params_.data_mask());
    const bool status_match = !strict_status_compare_
        || (exp.parity_error == frame.parity_error
            && exp.framing_error == frame.framing_error
            && exp.break_detect == frame.break_detect);

    if (!data_match || !status_match) {
        note_mismatch_(port, exp, frame);
        return;
    }

    if (verbose_) {
        scb_.note_pass("uart stream " + port + ": observed " + frame_string_(frame),
                       frame.end_tick);
    }
}

std::size_t ScbUartStream::observed_count(const std::string& port) const {
    const auto it = observed_.find(port);
    if (it == observed_.end()) {
        return 0u;
    }
    return it->second;
}

std::size_t ScbUartStream::expected_pending(const std::string& port) const {
    const auto it = expected_.find(port);
    if (it == expected_.end()) {
        return 0u;
    }
    return it->second.size();
}

void ScbUartStream::note_mismatch_(const std::string& port,
                                   const UartFrame& exp,
                                   const UartFrame& obs) {
    scb_.note_fail("uart stream " + port + ": expected " + frame_string_(exp)
                   + " observed " + frame_string_(obs),
                   obs.end_tick);
}

std::string ScbUartStream::frame_string_(const UartFrame& frame) {
    std::ostringstream oss;
    oss << "data=0x" << std::hex << std::setw(2) << std::setfill('0')
        << static_cast<unsigned>(frame.data)
        << std::dec
        << " parity_error=" << (frame.parity_error ? 1 : 0)
        << " framing_error=" << (frame.framing_error ? 1 : 0)
        << " break=" << (frame.break_detect ? 1 : 0);
    return oss.str();
}

} // namespace vip::uart

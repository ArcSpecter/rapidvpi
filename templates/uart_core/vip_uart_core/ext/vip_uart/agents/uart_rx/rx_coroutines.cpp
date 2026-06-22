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

#include "vip_uart/agents/uart_rx/rx.hpp"

#include "vip_common/common/logger.hpp"

namespace vip::uart {

UartRx::RunTask UartRx::agent(const unsigned idx) {
    if (idx >= ports_.size()) {
        co_return;
    }

    auto& port = ports_.at(idx);
    co_await drive_cts_(port);

    for (;;) {
        co_await drive_cts_(port);

        bool in_reset = false;
        co_await reset_asserted_(in_reset);
        if (in_reset) {
            co_await wait_clks_(params_.idle_poll_clks);
            continue;
        }

        bool line = params_.idle_high;
        co_await sample_line_(port, line);
        if (line == params_.idle_high) {
            co_await wait_clks_(params_.idle_poll_clks);
            continue;
        }

        UartFrame frame;
        co_await capture_frame_(port, frame);

        port.observed_count++;
        if (port.capture_enable) {
            port.history.push_back(frame);
        }
        if (scb_stream_ != nullptr) {
            scb_stream_->observe_frame(port.cfg.name, frame);
        }
        if (scb_rules_ != nullptr) {
            scb_rules_->observe_frame(port.cfg.name, frame);
        }

        if (verbose_) {
            vip::common::log_line("vip_uart_rx",
                                  "INFO",
                                  port.cfg.name + " observed byte "
                                      + std::to_string(static_cast<unsigned>(frame.data)));
        }
    }

    co_return;
}

UartRx::RunUserTask UartRx::wait_clks_(const unsigned clks) {
    const unsigned n = clks == 0u ? 1u : clks;
    co_await utils_.clock(static_cast<int>(n), 1);
    co_return;
}

UartRx::RunUserTask UartRx::sample_line_(PortState& port,
                                         bool& value,
                                         test::sim_tick_t* time_tick) {
    auto r = tb_.getCoRead();
    r.read(port.cfg.rx_net);
    co_await r;
    value = (r.getNum(port.cfg.rx_net) & 1u) != 0u;
    if (time_tick != nullptr) {
        *time_tick = r.getTime<test::ticks>();
    }
    co_return;
}

UartRx::RunUserTask UartRx::reset_asserted_(bool& asserted) {
    asserted = false;
    if (reset_net_.empty()) {
        co_return;
    }

    auto r = tb_.getCoRead();
    r.read(reset_net_);
    co_await r;
    const bool rst_value = (r.getNum(reset_net_) & 1u) != 0u;
    asserted = reset_active_low_ ? !rst_value : rst_value;
    co_return;
}

UartRx::RunUserTask UartRx::drive_cts_(PortState& port) {
    if (!port.cts_drive_enable || port.cfg.cts_net.empty()) {
        co_return;
    }

    const bool physical = active_to_physical(port.cts_active, port.cfg.cts_active_low);
    auto w = tb_.getCoWrite();
    w.write(port.cfg.cts_net, physical ? 1 : 0);
    co_await w;

    if (scb_rules_ != nullptr) {
        scb_rules_->observe_cts_drive(port.cfg.name, port.cts_active);
    }
    co_return;
}

UartRx::RunUserTask UartRx::capture_frame_(PortState& port, UartFrame& frame) {
    frame.data_bits = params_.data_bits;
    frame.stop_bits = params_.stop_bits;
    frame.parity = params_.parity;

    const bool start_level = !params_.idle_high;
    const bool stop_level = params_.idle_high;

    co_await wait_clks_(params_.sample_clk_index);

    bool start_sample = stop_level;
    test::sim_tick_t sample_time_tick = vip::common::INVALID_TICK;
    co_await sample_line_(port, start_sample, &sample_time_tick);
    frame.start_tick = sample_time_tick;
    if (start_sample != start_level) {
        frame.framing_error = true;
    }

    std::uint8_t data = 0u;
    for (unsigned bit = 0u; bit < params_.data_bits; ++bit) {
        co_await wait_clks_(params_.bit_clks);
        bool sample = false;
        co_await sample_line_(port, sample);

        const unsigned dst_bit = params_.lsb_first ? bit : (params_.data_bits - 1u - bit);
        if (sample) {
            data = static_cast<std::uint8_t>(data | static_cast<std::uint8_t>(1u << dst_bit));
        }
    }
    frame.data = static_cast<std::uint8_t>(data & params_.data_mask());

    if (params_.parity_enable()) {
        co_await wait_clks_(params_.bit_clks);
        bool parity_sample = false;
        co_await sample_line_(port, parity_sample);
        const bool expected = uart_parity_bit(frame.data, params_);
        frame.parity_error = parity_sample != expected;
    }

    bool all_stop_low = true;
    for (unsigned stop = 0u; stop < params_.stop_bits; ++stop) {
        co_await wait_clks_(params_.bit_clks);
        bool stop_sample = false;
        co_await sample_line_(port, stop_sample, &sample_time_tick);
        if (stop_sample != stop_level) {
            frame.framing_error = true;
        }
        all_stop_low = all_stop_low && (stop_sample == start_level);
    }

    frame.break_detect = frame.framing_error
        && frame.data == 0u
        && all_stop_low;
    frame.end_tick = sample_time_tick;
    co_return;
}

} // namespace vip::uart

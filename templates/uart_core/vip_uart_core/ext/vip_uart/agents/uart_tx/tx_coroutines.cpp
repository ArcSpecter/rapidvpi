// MIT License

// Copyright (c) 2026 Rovshan Rustamov

// Permission is hereby granted, free of charge, to any person obtaining a copy
// of this software and associated documentation files (the "Software"), to deal
// in the Software without restriction, including without limitation the rights
// to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
// copies of the Software, and to permit persons to whom the Software is
// furnished to do so, subject to the following conditions:

// The above copyright notice and this permission notice shall be included in all
// copies or substantial portions of the Software.

// THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
// IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
// FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
// AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
// LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
// OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE
// SOFTWARE.

#include "vip_uart/agents/uart_tx/tx.hpp"

#include "vip_common/common/logger.hpp"

namespace vip::uart {

UartTx::RunTask UartTx::agent(const unsigned idx) {
    if (idx >= ports_.size()) {
        co_return;
    }

    auto& port = ports_.at(idx);
    co_await drive_line_(port, params_.idle_high);

    for (;;) {
        bool in_reset = false;
        co_await reset_asserted_(in_reset);
        if (in_reset) {
            co_await drive_line_(port, params_.idle_high);
            co_await wait_ticks_(params_.idle_poll_ticks);
            continue;
        }

        if (port.pending.empty()) {
            co_await wait_ticks_(params_.idle_poll_ticks);
            continue;
        }

        bool rts_ready = true;
        co_await wait_rts_active_(port, rts_ready);
        if (!rts_ready) {
            co_await wait_ticks_(params_.idle_poll_ticks);
            continue;
        }

        TxItem item = port.pending.front();
        port.pending.pop_front();
        co_await send_item_(port, item);
        ticket_done_[item.ticket] = true;

        if (port.inter_frame_gap_ticks != 0u) {
            co_await wait_ticks_(port.inter_frame_gap_ticks);
        }
    }

    co_return;
}

UartTx::RunUserTask UartTx::drive_line_(PortState& port, const bool logical_level) {
    auto w = tb_.getCoWrite(0);
    w.write(port.cfg.tx_net, logical_level ? 1 : 0);
    co_await w;
    co_return;
}

UartTx::RunUserTask UartTx::wait_ticks_(const unsigned ticks) {
    const unsigned n = ticks == 0u ? 1u : ticks;
    co_await utils_.clock(static_cast<int>(n), 1);
    co_return;
}

UartTx::RunUserTask UartTx::wait_item_bit_(const TxItem& item) {
    if (item.use_time_delay) {
        co_await utils_.delay_ns(item.bit_time_ns);
    } else {
        co_await wait_ticks_(params_.bit_ticks);
    }
    co_return;
}

UartTx::RunUserTask UartTx::read_bit_(const std::string& net, bool& value) {
    auto r = tb_.getCoRead(0);
    r.read(net);
    co_await r;
    value = (r.getNum(net) & 1u) != 0u;
    co_return;
}

UartTx::RunUserTask UartTx::reset_asserted_(bool& asserted) {
    asserted = false;
    if (reset_net_.empty()) {
        co_return;
    }

    bool rst_value = false;
    co_await read_bit_(reset_net_, rst_value);
    asserted = reset_active_low_ ? !rst_value : rst_value;
    co_return;
}

UartTx::RunUserTask UartTx::wait_rts_active_(PortState& port, bool& active) {
    active = true;
    if (!port.respect_rts || port.cfg.rts_net.empty()) {
        co_return;
    }

    unsigned waited = 0u;
    for (;;) {
        bool physical = false;
        co_await read_bit_(port.cfg.rts_net, physical);
        active = physical_to_active(physical, port.cfg.rts_active_low);
        if (active) {
            co_return;
        }

        if (scb_rules_ != nullptr && waited != 0u) {
            scb_rules_->observe_rts_blocked(port.cfg.name, waited);
        }

        if (port.rts_wait_timeout_ticks != 0u && waited >= port.rts_wait_timeout_ticks) {
            if (scb_rules_ != nullptr) {
                scb_rules_->observe_rts_timeout(port.cfg.name, waited);
            }
            active = false;
            co_return;
        }

        waited += params_.idle_poll_ticks;
        co_await wait_ticks_(params_.idle_poll_ticks);
    }
}

UartTx::RunUserTask UartTx::send_item_(PortState& port, TxItem item) {
    UartFrame sent = item.frame;

    const bool start_level = !params_.idle_high;
    const bool stop_level = params_.idle_high;

    if (item.use_time_delay && item.align_to_clock_phase) {
        co_await utils_.clock_to_write(1, 1);
        if (item.phase_offset_ps != 0u) {
            co_await utils_.delay_ns(static_cast<double>(item.phase_offset_ps) / 1000.0);
        }
    }

    sent.start_time_ns = static_cast<double>(vip::common::sim_time_ns());
    co_await drive_line_(port, start_level);
    co_await wait_item_bit_(item);

    for (unsigned bit = 0u; bit < params_.data_bits; ++bit) {
        const unsigned src_bit = params_.lsb_first ? bit : (params_.data_bits - 1u - bit);
        const bool value = ((sent.data >> src_bit) & 1u) != 0u;
        co_await drive_line_(port, value);
        co_await wait_item_bit_(item);
    }

    if (params_.parity_enable()) {
        bool parity_bit = uart_parity_bit(sent.data, params_);
        if (item.force_bad_parity) {
            parity_bit = !parity_bit;
        }
        co_await drive_line_(port, parity_bit);
        co_await wait_item_bit_(item);
    }

    for (unsigned stop = 0u; stop < params_.stop_bits; ++stop) {
        const bool stop_bit = item.force_bad_stop ? !stop_level : stop_level;
        co_await drive_line_(port, stop_bit);
        co_await wait_item_bit_(item);
    }

    co_await drive_line_(port, params_.idle_high);
    sent.end_time_ns = static_cast<double>(vip::common::sim_time_ns());
    port.history.push_back(sent);

    if (verbose_) {
        vip::common::log_line("vip_uart_tx",
                              "INFO",
                              port.cfg.name + " sent byte "
                                  + std::to_string(static_cast<unsigned>(sent.data)));
    }

    co_return;
}

} // namespace vip::uart

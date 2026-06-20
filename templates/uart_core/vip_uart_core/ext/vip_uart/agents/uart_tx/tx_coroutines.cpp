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
            co_await wait_clks_(params_.idle_poll_clks);
            continue;
        }

        if (port.pending.empty()) {
            co_await wait_clks_(params_.idle_poll_clks);
            continue;
        }

        bool rts_ready = true;
        co_await wait_rts_active_(port, rts_ready);
        if (!rts_ready) {
            co_await wait_clks_(params_.idle_poll_clks);
            continue;
        }

        TxItem item = port.pending.front();
        port.pending.pop_front();
        co_await send_item_(port, item);
        ticket_done_[item.ticket] = true;

        if (port.inter_frame_gap_clks != 0u) {
            co_await wait_clks_(port.inter_frame_gap_clks);
        }
    }

    co_return;
}

UartTx::RunUserTask UartTx::drive_line_(PortState& port, const bool logical_level) {
    auto w = tb_.getCoWrite();
    w.write(port.cfg.tx_net, logical_level ? 1 : 0);
    co_await w;
    co_return;
}

UartTx::RunUserTask UartTx::wait_clks_(const unsigned clks) {
    const unsigned n = clks == 0u ? 1u : clks;
    co_await utils_.clock(static_cast<int>(n), 1);
    co_return;
}

UartTx::RunUserTask UartTx::wait_item_bit_(const TxItem& item) {
    if (item.use_time_delay) {
        co_await utils_.delay<test::ns>(item.bit_time_ns);
    } else {
        co_await wait_clks_(params_.bit_clks);
    }
    co_return;
}

UartTx::RunUserTask UartTx::read_bit_(const std::string& net, bool& value) {
    auto r = tb_.getCoRead();
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

    unsigned waited_clks = 0u;
    for (;;) {
        bool physical = false;
        co_await read_bit_(port.cfg.rts_net, physical);
        active = physical_to_active(physical, port.cfg.rts_active_low);
        if (active) {
            co_return;
        }

        if (scb_rules_ != nullptr && waited_clks != 0u) {
            scb_rules_->observe_rts_blocked(port.cfg.name, waited_clks);
        }

        if (port.rts_wait_timeout_clks != 0u && waited_clks >= port.rts_wait_timeout_clks) {
            if (scb_rules_ != nullptr) {
                scb_rules_->observe_rts_timeout(port.cfg.name, waited_clks);
            }
            active = false;
            co_return;
        }

        waited_clks += params_.idle_poll_clks;
        co_await wait_clks_(params_.idle_poll_clks);
    }
}

UartTx::RunUserTask UartTx::send_item_(PortState& port, TxItem item) {
    UartFrame sent = item.frame;

    const bool start_level = !params_.idle_high;
    const bool stop_level = params_.idle_high;

    if (item.use_time_delay && item.align_to_clock_phase) {
        co_await utils_.clock_to_write(1, 1);
        if (item.phase_offset_ps != 0u) {
            co_await utils_.delay<test::ps>(item.phase_offset_ps);
        }
    }

    sent.start_tick = vip::common::sim_time_ticks();
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
    sent.end_tick = vip::common::sim_time_ticks();
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

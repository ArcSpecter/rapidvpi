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

#include "agents/uart_core_intf/intf.hpp"

#include <utility>

#include "pindefs.hpp"
#include "scoreboard/scb_uart_core.hpp"
#include "vip_common/common/logger.hpp"

namespace test {

UartCoreIntf::UartCoreIntf(TestBase& tb,
                           std::string clock_net,
                           std::string reset_net,
                           const bool reset_active_low)
    : tb_(tb)
    , utils_(tb, clock_net)
    , clock_net_(std::move(clock_net))
    , reset_net_(std::move(reset_net))
    , reset_active_low_(reset_active_low) {}

void UartCoreIntf::reset_case() {
    event_counts_ = UartCoreEventCounts{};
}

UartCoreIntf::RunTask UartCoreIntf::monitor_events() {
    for (;;) {
        co_await utils_.clock(1, 1);

        auto r = tb_.getCoRead();
        r.read(reset_net_);
        r.read(event_rx_overrun);
        r.read(event_rx_frame_error);
        r.read(event_rx_parity_error);
        r.read(event_rx_break_detect);
        r.read(event_tx_done);
        co_await r;

        const bool rst_value = (r.getNum(reset_net_) & 1u) != 0u;
        const bool in_reset = reset_active_low_ ? !rst_value : rst_value;
        if (in_reset) {
            event_counts_ = UartCoreEventCounts{};
            continue;
        }

        event_counts_.rx_overrun += (r.getNum(event_rx_overrun) & 1u) != 0u ? 1u : 0u;
        event_counts_.rx_frame_error += (r.getNum(event_rx_frame_error) & 1u) != 0u ? 1u : 0u;
        event_counts_.rx_parity_error += (r.getNum(event_rx_parity_error) & 1u) != 0u ? 1u : 0u;
        event_counts_.rx_break_detect += (r.getNum(event_rx_break_detect) & 1u) != 0u ? 1u : 0u;
        event_counts_.tx_done += (r.getNum(event_tx_done) & 1u) != 0u ? 1u : 0u;
    }

    co_return;
}

UartCoreIntf::RunUserTask UartCoreIntf::drive_idle() {
    auto w = tb_.getCoWrite();
    w.write(cfg_enable, 0);
    w.write(cfg_rx_enable, 0);
    w.write(cfg_tx_enable, 0);
    w.write(cfg_baud_inc, BAUD_INC_DISABLED);
    w.write(cfg_parity_mode, UART_PARITY_NONE);
    w.write(cfg_stop_bits, UART_STOP_1);
    w.write(cfg_data_bits, UART_DATA_8);
    w.write(cfg_hw_flow_enable, 0);

    w.write(ctrl_rx_fifo_clear, 0);
    w.write(ctrl_tx_fifo_clear, 0);

    w.write(tx_byte_valid, 0);
    w.write(tx_byte_data, 0);
    w.write(rx_byte_ready, 0);
    co_await w;
    co_return;
}

UartCoreIntf::RunUserTask UartCoreIntf::apply_config(const UartCoreConfig& cfg) {
    co_await utils_.clock_to_write(1, 0);
    co_await write_config_(cfg);
    co_return;
}

UartCoreIntf::RunUserTask UartCoreIntf::pulse_rx_fifo_clear() {
    co_await pulse_net_(ctrl_rx_fifo_clear);
    co_return;
}

UartCoreIntf::RunUserTask UartCoreIntf::pulse_tx_fifo_clear() {
    co_await pulse_net_(ctrl_tx_fifo_clear);
    co_return;
}

UartCoreIntf::RunUserTask UartCoreIntf::push_tx_byte(const std::uint8_t data,
                                                     const unsigned timeout_cycles) {
    bool accepted = false;
    co_await try_push_tx_byte(data, accepted, timeout_cycles);

    if (!accepted) {
        if (scb_core_ != nullptr) {
            scb_core_->note_fail("tx_byte handshake timed out");
        }
    }

    co_return;
}

UartCoreIntf::RunUserTask UartCoreIntf::try_push_tx_byte(const std::uint8_t data,
                                                         bool& accepted,
                                                         const unsigned timeout_cycles) {
    accepted = false;
    bool at_low_phase = true;

    co_await utils_.clock_to_write(1, 0);
    co_await write_tx_valid_(false, data);

    co_await utils_.write_barrier();
    co_await write_tx_valid_(true, data);

    for (unsigned cycle = 0u; cycle < timeout_cycles; ++cycle) {
        auto r = tb_.getCoRead();
        r.read(tx_byte_ready);
        co_await r;

        const bool ready_for_edge = (r.getNum(tx_byte_ready) & 1u) != 0u;
        co_await utils_.clock(1, 1);
        at_low_phase = false;

        if (ready_for_edge) {
            accepted = true;
            break;
        }

        co_await utils_.clock(1, 0);
        at_low_phase = true;
    }

    if (!at_low_phase) {
        co_await utils_.clock_to_write(1, 0);
    } else {
        co_await utils_.write_barrier();
    }
    co_await write_tx_valid_(false, data);

    co_return;
}

UartCoreIntf::RunUserTask UartCoreIntf::pop_rx_byte(UartCoreRxByte& rec,
                                                    const unsigned timeout_cycles) {
    rec = UartCoreRxByte{};

    co_await utils_.clock_to_write(1, 0);
    {
        auto w = tb_.getCoWrite();
        w.write(rx_byte_ready, 0);
        co_await w;
    }

    for (unsigned cycle = 0u; cycle < timeout_cycles; ++cycle) {
        auto r = tb_.getCoRead();
        r.read(rx_byte_valid);
        r.read(rx_byte_data);
        r.read(rx_byte_frame_error);
        r.read(rx_byte_parity_error);
        r.read(rx_byte_break_detect);
        co_await r;

        if ((r.getNum(rx_byte_valid) & 1u) != 0u) {
            rec.valid = true;
            rec.data = static_cast<std::uint8_t>(r.getNum(rx_byte_data) & 0xffu);
            rec.frame_error = (r.getNum(rx_byte_frame_error) & 1u) != 0u;
            rec.parity_error = (r.getNum(rx_byte_parity_error) & 1u) != 0u;
            rec.break_detect = (r.getNum(rx_byte_break_detect) & 1u) != 0u;
            rec.time_tick = r.getTime<test::ticks>();
            break;
        }

        co_await utils_.clock(1, 1);
    }

    if (rec.valid) {
        co_await utils_.clock_to_write(1, 0);
        {
            auto w = tb_.getCoWrite();
            w.write(rx_byte_ready, 1);
            co_await w;
        }

        co_await utils_.clock(1, 1);

        co_await utils_.clock_to_write(1, 0);
        {
            auto w = tb_.getCoWrite();
            w.write(rx_byte_ready, 0);
            co_await w;
        }
    }

    if (scb_core_ != nullptr) {
        if (rec.valid) {
            scb_core_->observe_rx_byte(rec);
        } else {
            scb_core_->note_fail("rx_byte pop timed out");
        }
    }

    co_return;
}

UartCoreIntf::RunUserTask UartCoreIntf::set_rx_ready(const bool ready) {
    co_await utils_.clock_to_write(1, 0);
    auto w = tb_.getCoWrite();
    w.write(rx_byte_ready, ready ? 1 : 0);
    co_await w;
    co_return;
}

UartCoreIntf::RunUserTask UartCoreIntf::sample_status(UartCoreStatus& status) {
    auto r = tb_.getCoRead();
    r.read(tx_byte_ready);
    r.read(rx_byte_valid);
    r.read(rx_fifo_level);
    r.read(tx_fifo_level);
    r.read(rx_fifo_empty);
    r.read(rx_fifo_full);
    r.read(tx_fifo_empty);
    r.read(tx_fifo_full);
    r.read(rx_busy);
    r.read(tx_busy);
    r.read(cts_active);
    r.read(rts_active);
    r.read(cts_blocked);
    r.read(event_rx_overrun);
    r.read(event_rx_frame_error);
    r.read(event_rx_parity_error);
    r.read(event_rx_break_detect);
    r.read(event_tx_done);
    co_await r;

    status.tx_byte_ready = (r.getNum(tx_byte_ready) & 1u) != 0u;
    status.rx_byte_valid = (r.getNum(rx_byte_valid) & 1u) != 0u;
    status.rx_level = static_cast<unsigned>(r.getNum(rx_fifo_level));
    status.tx_level = static_cast<unsigned>(r.getNum(tx_fifo_level));
    status.rx_empty = (r.getNum(rx_fifo_empty) & 1u) != 0u;
    status.rx_full = (r.getNum(rx_fifo_full) & 1u) != 0u;
    status.tx_empty = (r.getNum(tx_fifo_empty) & 1u) != 0u;
    status.tx_full = (r.getNum(tx_fifo_full) & 1u) != 0u;
    status.rx_busy = (r.getNum(rx_busy) & 1u) != 0u;
    status.tx_busy = (r.getNum(tx_busy) & 1u) != 0u;
    status.cts_active = (r.getNum(cts_active) & 1u) != 0u;
    status.rts_active = (r.getNum(rts_active) & 1u) != 0u;
    status.cts_blocked = (r.getNum(cts_blocked) & 1u) != 0u;
    status.event_rx_overrun = (r.getNum(event_rx_overrun) & 1u) != 0u;
    status.event_rx_frame_error = (r.getNum(event_rx_frame_error) & 1u) != 0u;
    status.event_rx_parity_error = (r.getNum(event_rx_parity_error) & 1u) != 0u;
    status.event_rx_break_detect = (r.getNum(event_rx_break_detect) & 1u) != 0u;
    status.event_tx_done = (r.getNum(event_tx_done) & 1u) != 0u;
    co_return;
}

UartCoreIntf::RunUserTask UartCoreIntf::wait_tx_idle(const unsigned timeout_cycles) {
    for (unsigned cycle = 0u; cycle < timeout_cycles; ++cycle) {
        co_await utils_.clock(1, 1);

        UartCoreStatus status{};
        co_await sample_status(status);
        if (status.tx_empty && !status.tx_busy) {
            co_return;
        }
    }

    if (scb_core_ != nullptr) {
        scb_core_->note_fail("tx idle wait timed out");
    }
    co_return;
}

UartCoreIntf::RunUserTask UartCoreIntf::write_config_(const UartCoreConfig& cfg) {
    auto w = tb_.getCoWrite();
    w.write(cfg_enable, cfg.enable ? 1 : 0);
    w.write(cfg_rx_enable, cfg.rx_enable ? 1 : 0);
    w.write(cfg_tx_enable, cfg.tx_enable ? 1 : 0);
    w.write(cfg_baud_inc, cfg.baud_inc);
    w.write(cfg_parity_mode, cfg.parity_mode & 0x3u);
    w.write(cfg_stop_bits, cfg.stop_bits & 0x3u);
    w.write(cfg_data_bits, cfg.data_bits & 0x3u);
    w.write(cfg_hw_flow_enable, cfg.hw_flow_enable ? 1 : 0);
    co_await w;
    co_return;
}

UartCoreIntf::RunUserTask UartCoreIntf::write_tx_valid_(const bool valid,
                                                        const std::uint8_t data) {
    auto w = tb_.getCoWrite();
    w.write(tx_byte_data, data);
    w.write(tx_byte_valid, valid ? 1 : 0);
    co_await w;
    co_return;
}

UartCoreIntf::RunUserTask UartCoreIntf::pulse_net_(const std::string& net) {
    co_await utils_.clock_to_write(1, 0);
    {
        auto w = tb_.getCoWrite();
        w.write(net, 1);
        co_await w;
    }

    co_await utils_.clock_to_write(1, 0);
    {
        auto w = tb_.getCoWrite();
        w.write(net, 0);
        co_await w;
    }
    co_return;
}

} // namespace test

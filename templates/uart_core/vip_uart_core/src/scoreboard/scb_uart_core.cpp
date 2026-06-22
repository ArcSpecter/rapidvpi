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

#include "scoreboard/scb_uart_core.hpp"

#include <iomanip>
#include <sstream>

namespace test {

ScbUartCore::ScbUartCore(vip::common::Scoreboard& scb)
    : scb_(scb) {}

void ScbUartCore::reset_case() {
    expected_rx_.clear();
    expected_tx_.clear();
}

void ScbUartCore::end_case_check(const bool fail_on_outstanding) {
    if (!fail_on_outstanding) {
        return;
    }

    if (!expected_rx_.empty()) {
        scb_.note_fail("uart_core RX byte-side outstanding expected records="
                       + std::to_string(expected_rx_.size()));
    }
    if (!expected_tx_.empty()) {
        scb_.note_fail("uart_core TX serial-side outstanding expected bytes="
                       + std::to_string(expected_tx_.size()));
    }
}

void ScbUartCore::expect_rx_byte(UartCoreRxByte rec) {
    rec.valid = true;
    expected_rx_.push_back(rec);
    if (verbose_) {
        scb_.note_info("uart_core expect RX byte-side " + rx_record_string_(rec));
    }
}

void ScbUartCore::expect_rx_bytes(const std::vector<std::uint8_t>& data) {
    for (const std::uint8_t byte : data) {
        expect_rx_byte(make_expected_rx_byte(byte));
    }
}

void ScbUartCore::observe_rx_byte(const UartCoreRxByte& rec) {
    if (!rec.valid) {
        scb_.note_fail("uart_core observed invalid RX byte-side record");
        return;
    }

    if (expected_rx_.empty()) {
        scb_.note_fail("uart_core unexpected RX byte-side record "
                       + rx_record_string_(rec),
                       rec.time_tick);
        return;
    }

    const UartCoreRxByte exp = expected_rx_.front();
    expected_rx_.pop_front();

    const bool match = exp.data == rec.data
        && exp.frame_error == rec.frame_error
        && exp.parity_error == rec.parity_error
        && exp.break_detect == rec.break_detect;

    if (!match) {
        scb_.note_fail("uart_core RX byte-side mismatch expected "
                       + rx_record_string_(exp)
                       + " observed " + rx_record_string_(rec),
                       rec.time_tick);
        return;
    }

    if (verbose_) {
        scb_.note_pass("uart_core observed RX byte-side " + rx_record_string_(rec),
                       rec.time_tick);
    }
}

void ScbUartCore::expect_tx_byte(const std::uint8_t data) {
    expected_tx_.push_back(data);
    if (verbose_) {
        scb_.note_info("uart_core expect TX serial byte "
                       + std::to_string(static_cast<unsigned>(data)));
    }
}

void ScbUartCore::expect_tx_bytes(const std::vector<std::uint8_t>& data) {
    for (const std::uint8_t byte : data) {
        expect_tx_byte(byte);
    }
}

void ScbUartCore::observe_uart_tx_frame(const vip::uart::UartFrame& frame) {
    if (expected_tx_.empty()) {
        scb_.note_fail("uart_core unexpected UART TX frame data="
                       + std::to_string(static_cast<unsigned>(frame.data)),
                       frame.end_tick);
        return;
    }

    const std::uint8_t exp = expected_tx_.front();
    expected_tx_.pop_front();

    if (frame.data != exp || frame.framing_error || frame.parity_error || frame.break_detect) {
        std::ostringstream oss;
        oss << "uart_core TX serial mismatch expected data=0x"
            << std::hex << std::setw(2) << std::setfill('0')
            << static_cast<unsigned>(exp)
            << " observed data=0x" << std::setw(2)
            << static_cast<unsigned>(frame.data)
            << std::dec
            << " framing_error=" << (frame.framing_error ? 1 : 0)
            << " parity_error=" << (frame.parity_error ? 1 : 0)
            << " break=" << (frame.break_detect ? 1 : 0);
        scb_.note_fail(oss.str(), frame.end_tick);
        return;
    }

    if (verbose_) {
        scb_.note_pass("uart_core observed TX serial byte "
                       + std::to_string(static_cast<unsigned>(frame.data)),
                       frame.end_tick);
    }
}

void ScbUartCore::check_idle_status(const UartCoreStatus& status, const std::string& label) {
    if (status.rx_level != 0u || !status.rx_empty || status.rx_full) {
        scb_.note_fail(label + ": RX FIFO idle status mismatch");
    }
    if (status.tx_level != 0u || !status.tx_empty || status.tx_full) {
        scb_.note_fail(label + ": TX FIFO idle status mismatch");
    }
    if (status.rx_busy) {
        scb_.note_fail(label + ": rx_busy asserted while idle expected");
    }
    if (status.tx_busy) {
        scb_.note_fail(label + ": tx_busy asserted while idle expected");
    }
    if (status.cts_blocked) {
        scb_.note_fail(label + ": cts_blocked asserted while idle expected");
    }
}

void ScbUartCore::note_fail(const std::string& msg) {
    scb_.note_fail("uart_core: " + msg);
}

std::string ScbUartCore::rx_record_string_(const UartCoreRxByte& rec) {
    std::ostringstream oss;
    oss << "data=0x" << std::hex << std::setw(2) << std::setfill('0')
        << static_cast<unsigned>(rec.data)
        << std::dec
        << " frame_error=" << (rec.frame_error ? 1 : 0)
        << " parity_error=" << (rec.parity_error ? 1 : 0)
        << " break=" << (rec.break_detect ? 1 : 0);
    return oss.str();
}

} // namespace test

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

#include "test.hpp"

#include "cases/tc_basic.hpp"
#include "cases/tc_cfg.hpp"
#include "cases/tc_error.hpp"
#include "cases/tc_fifo.hpp"
#include "cases/tc_flow_ctrl.hpp"
#include "cases/tc_phase.hpp"
#include "cases/tc_reset.hpp"
#include "cases/tc_stress_no_cts.hpp"
#include "core.hpp"

namespace test {

Test::Test()
    : dutName(dut_name)
    , scb()
    , utils(*this, clk)
    , por(*this, rst_n)
    , clock_agent(*this, clk, "clk_run")
    , uart_params(make_uart_params())
    , scb_uart_stream(scb, uart_params)
    , scb_uart_rules(scb)
    , scb_core(scb)
    , uart_peer_tx(*this, clk, rst_n, make_uart_rx_serial_port(), uart_params)
    , uart_peer_rx(*this, clk, rst_n, make_uart_tx_serial_port(), uart_params)
    , core_intf(*this, clk, rst_n)
    , runner(*this) {
    runner.register_tasks();

    registerTest("uart_peer_tx_run", [this]() { return uart_peer_tx.agent(0).handle; });
    registerTest("uart_peer_rx_run", [this]() { return uart_peer_rx.agent(0).handle; });
    registerTest("uart_core_intf_events_run", [this]() { return core_intf.monitor_events().handle; });

    uart_peer_tx.attach_scoreboards(nullptr, &scb_uart_rules);
    uart_peer_rx.attach_scoreboards(&scb_uart_stream, &scb_uart_rules);
    core_intf.attach_scoreboard(&scb_core);

    scb.enable_print_info(true);
    scb.enable_print_pass(true);
    scb_uart_stream.set_verbose(false);
    scb_uart_rules.set_warn_only(false);
    scb_core.set_verbose(false);

    runner.set_before_case_hook([this](const vip::common::Runner::CaseDesc& c) {
        scb.reset_case();
        scb_uart_stream.reset_case();
        scb_uart_rules.reset_case();
        scb_core.reset_case();
        uart_peer_tx.reset_case();
        uart_peer_rx.reset_case();
        core_intf.reset_case();

        scb.start_case(c.name);
    });

    runner.set_after_case_hook([this](const vip::common::Runner::CaseDesc&) {
        scb_core.end_case_check(true);
        scb_uart_stream.end_case_check(true);
        scb_uart_rules.end_case_check(false);

        scb.end_case();
        scb.print_case_summary();
        scb.print_total_summary();
    });

    register_tc_basic(*this, {"smoke", "regression"}, true, "tc_basic");
    register_tc_cfg(*this, {"config", "regression"}, true, "tc_cfg");
    register_tc_fifo(*this, {"fifo", "regression"}, true, "tc_fifo");
    register_tc_error(*this, {"error", "regression"}, true, "tc_error");
    register_tc_flow_ctrl(*this, {"flow_ctrl", "regression"}, true, "tc_flow_ctrl");
    register_tc_reset(*this, {"reset", "regression"}, true, "tc_reset");
    register_tc_phase(*this, {"phase", "rx", "regression"}, true, "tc_phase");
    register_tc_stress_no_cts(*this, {"stress", "no_cts", "regression"}, true, "tc_stress_no_cts");

    runner.set_plan({
        "tc_basic",
        // "tc_cfg",
        // "tc_fifo",
        // "tc_error",
        // "tc_flow_ctrl",
        // "tc_reset",
        // "tc_phase",
        // "tc_stress_no_cts",
    });
}

} // namespace test

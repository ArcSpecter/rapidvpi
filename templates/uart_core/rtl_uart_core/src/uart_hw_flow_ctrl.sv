// MIT License
//
// Copyright (c) 2026 Rovshan Rustamov
//
// Permission is hereby granted, free of charge, to any person obtaining a copy
// of this software and associated documentation files (the "Software"), to deal
// in the Software without restriction, including without limitation the rights
// to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
// copies of the Software, and to permit persons to whom the Software is
// furnished to do so, subject to the following conditions:
//
// The above copyright notice and this permission notice shall be included in all
// copies or substantial portions of the Software.
//
// THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
// IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
// FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
// AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
// LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
// OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE
// SOFTWARE.

`default_nettype none

// ----------------------------------------------------------------------------
// Debug print macro:
// - In simulation: expands to `DPRINT(...)
// - In synthesis: expands to nothing (arguments removed by preprocessor)
// ----------------------------------------------------------------------------
`ifndef DPRINT
`ifndef SYNTHESIS
`define DPRINT(stmt) stmt
`else
`define DPRINT(stmt)
`endif
`endif

module uart_hw_flow_ctrl #(
  parameter int unsigned RX_LEVEL_W         = 5,
  parameter bit          RTS_ACTIVE_LOW     = 1'b1,
  parameter bit          CTS_ACTIVE_LOW     = 1'b1,
  parameter int unsigned RTS_DEASSERT_LEVEL = 14,
  parameter int unsigned RTS_ASSERT_LEVEL   = 8,
  // ================================================================
  // DEBUG CONTROL
  // ================================================================
  parameter bit RTL_DBG = 1'b1
) (
  input  wire                    clk,
  input  wire                    rst_n,
  input  wire                    cfg_enable,
  input  wire                    cfg_rx_enable,
  input  wire                    cfg_hw_flow_enable,
  input  wire                    uart_cts_i,
  input  wire [RX_LEVEL_W-1:0]   rx_fifo_level,
  input  wire                    tx_launch_wanted,
  output wire                    tx_start_allowed,
  output wire                    uart_rts_o,
  output wire                    cts_active,
  output wire                    rts_active,
  output wire                    cts_blocked
);

  localparam int unsigned CTS_SYNC_STAGES = 3;
  localparam logic CTS_RESET_LEVEL = CTS_ACTIVE_LOW ? 1'b0 : 1'b1;

  logic [CTS_SYNC_STAGES-1:0] cts_sync_q;
  logic                       rts_active_q;
  logic                       cts_active_raw;
  logic                       rts_active_selected;

`ifndef SYNTHESIS
  logic dbg_cfg_hw_flow_enable_q;
  logic dbg_cts_active_q;
  logic dbg_rts_active_q;
  logic dbg_cts_blocked_q;
`endif

  always_ff @(posedge clk) begin
    if (!rst_n) begin
      cts_sync_q <= {CTS_SYNC_STAGES{CTS_RESET_LEVEL}};
    end else begin
      cts_sync_q <= {cts_sync_q[CTS_SYNC_STAGES-2:0], uart_cts_i};
    end
  end

  always_ff @(posedge clk) begin
    if (!rst_n) begin
      rts_active_q <= 1'b1;
    end else if (!cfg_enable || !cfg_rx_enable) begin
      rts_active_q <= 1'b0;
    end else if (!cfg_hw_flow_enable) begin
      rts_active_q <= 1'b1;
    end else if (rts_active_q && (int'(rx_fifo_level) >= RTS_DEASSERT_LEVEL)) begin
      rts_active_q <= 1'b0;
    end else if (!rts_active_q && (int'(rx_fifo_level) <= RTS_ASSERT_LEVEL)) begin
      rts_active_q <= 1'b1;
    end
  end

  assign cts_active_raw = CTS_ACTIVE_LOW ? !cts_sync_q[CTS_SYNC_STAGES-1] :
                                           cts_sync_q[CTS_SYNC_STAGES-1];

  assign cts_active      = cfg_hw_flow_enable ? cts_active_raw : 1'b1;
  assign tx_start_allowed = !cfg_hw_flow_enable || cts_active_raw;
  assign cts_blocked     = cfg_hw_flow_enable && tx_launch_wanted && !cts_active_raw;

  assign rts_active_selected = cfg_hw_flow_enable ? rts_active_q :
                                                    (cfg_enable && cfg_rx_enable);
  assign rts_active = rts_active_selected;
  assign uart_rts_o = RTS_ACTIVE_LOW ? !rts_active_selected : rts_active_selected;

`ifndef SYNTHESIS
  always_ff @(posedge clk) begin
    if (!rst_n) begin
      dbg_cfg_hw_flow_enable_q <= 1'b0;
      dbg_cts_active_q         <= 1'b1;
      dbg_rts_active_q         <= 1'b1;
      dbg_cts_blocked_q        <= 1'b0;
    end else begin
      if (RTL_DBG) begin
        if (cfg_hw_flow_enable && !dbg_cfg_hw_flow_enable_q) begin
          `DPRINT($display(
            "[RTL][INFO][%0t] %m: hardware flow control enabled, rx_level=%0d",
            $time,
            rx_fifo_level
          ));
        end

        if (!cfg_hw_flow_enable && dbg_cfg_hw_flow_enable_q) begin
          `DPRINT($display(
            "[RTL][INFO][%0t] %m: hardware flow control disabled, CTS ignored",
            $time
          ));
        end

        if (cts_active != dbg_cts_active_q) begin
          `DPRINT($display(
            "[RTL][INFO][%0t] %m: CTS active changed to %0b",
            $time,
            cts_active
          ));
        end

        if (rts_active != dbg_rts_active_q) begin
          `DPRINT($display(
            "[RTL][INFO][%0t] %m: RTS active changed to %0b, rx_level=%0d thresholds assert=%0d deassert=%0d",
            $time,
            rts_active,
            rx_fifo_level,
            RTS_ASSERT_LEVEL,
            RTS_DEASSERT_LEVEL
          ));
        end

        if (cts_blocked && !dbg_cts_blocked_q) begin
          `DPRINT($display(
            "[RTL][WARN][%0t] %m: TX launch blocked by inactive CTS",
            $time
          ));
        end

        if (!cts_blocked && dbg_cts_blocked_q) begin
          `DPRINT($display(
            "[RTL][INFO][%0t] %m: CTS block cleared",
            $time
          ));
        end
      end

      dbg_cfg_hw_flow_enable_q <= cfg_hw_flow_enable;
      dbg_cts_active_q         <= cts_active;
      dbg_rts_active_q         <= rts_active;
      dbg_cts_blocked_q        <= cts_blocked;
    end
  end
`endif

endmodule

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

module uart_rx_sync #(
  // ================================================================
  // DEBUG CONTROL
  // ================================================================
  parameter bit RTL_DBG         = 1'b0,
  parameter bit RTL_DBG_TIME_NS = 1'b1,
  parameter bit RTL_DBG_TIME_US = 1'b0,
  parameter bit RTL_DBG_TIME_MS = 1'b0
) (
  input  wire clk,
  input  wire rst_n,
  input  wire uart_rx_i,
  output wire rx_sync_o
);

  localparam int unsigned SYNC_STAGES = 3;

  logic [SYNC_STAGES-1:0] rx_sync_q;

`ifndef SYNTHESIS
  localparam string RTL_DBG_TIMEUNIT_STR = RTL_DBG_TIME_MS ? "ms" :
                                           RTL_DBG_TIME_US ? "us" :
                                           RTL_DBG_TIME_NS ? "ns" : "ticks";

  logic dbg_activity_seen_q;
  logic dbg_rx_sync_d_q;

  function automatic real rtl_dbg_time();
    begin
      if (RTL_DBG_TIME_MS) begin
        rtl_dbg_time = $realtime / 1_000_000.0;
      end else if (RTL_DBG_TIME_US) begin
        rtl_dbg_time = $realtime / 1_000.0;
      end else begin
        rtl_dbg_time = $realtime;
      end
    end
  endfunction
`endif

  always_ff @(posedge clk) begin
    if (!rst_n) begin
      rx_sync_q <= {SYNC_STAGES{1'b1}};
    end else begin
      rx_sync_q <= {rx_sync_q[SYNC_STAGES-2:0], uart_rx_i};
    end
  end

`ifndef SYNTHESIS
  always_ff @(posedge clk) begin
    if (!rst_n) begin
      dbg_activity_seen_q <= 1'b0;
      dbg_rx_sync_d_q     <= 1'b1;
    end else begin
      if (RTL_DBG && !dbg_activity_seen_q &&
          (rx_sync_q[SYNC_STAGES-1] != dbg_rx_sync_d_q)) begin
        `DPRINT($display(
          "[RTL][INFO][%0.0f %s] %m: first synchronized RX activity observed, rx_sync_o=%0b",
          rtl_dbg_time(),
          RTL_DBG_TIMEUNIT_STR,
          rx_sync_q[SYNC_STAGES-1]
        ));
      end

      if (rx_sync_q[SYNC_STAGES-1] != dbg_rx_sync_d_q) begin
        dbg_activity_seen_q <= 1'b1;
      end

      dbg_rx_sync_d_q <= rx_sync_q[SYNC_STAGES-1];
    end
  end
`endif

  assign rx_sync_o = rx_sync_q[SYNC_STAGES-1];

endmodule

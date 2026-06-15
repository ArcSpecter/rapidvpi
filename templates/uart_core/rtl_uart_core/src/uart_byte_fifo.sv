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

module uart_byte_fifo #(
  parameter int unsigned DATA_W = 8,
  parameter int unsigned DEPTH  = 16,
  // ================================================================
  // DEBUG CONTROL
  // ================================================================
  parameter bit RTL_DBG         = 1'b0,
  parameter bit RTL_DBG_TIME_NS = 1'b1,
  parameter bit RTL_DBG_TIME_US = 1'b0,
  parameter bit RTL_DBG_TIME_MS = 1'b0
) (
  input  wire                             clk,
  input  wire                             rst_n,
  input  wire                             clear,
  input  wire                             push_valid,
  output wire                             push_ready,
  input  wire [DATA_W-1:0]                push_data,
  output wire                             pop_valid,
  input  wire                             pop_ready,
  output wire [DATA_W-1:0]                pop_data,
  output wire [$clog2(DEPTH + 1)-1:0]     level,
  output wire                             empty,
  output wire                             full
);

  localparam int unsigned LEVEL_W = $clog2(DEPTH + 1);
  localparam int unsigned PTR_W   = (DEPTH <= 2) ? 1 : $clog2(DEPTH);

  logic [DATA_W-1:0]  mem_q [0:DEPTH-1];
  logic [PTR_W-1:0]   wr_ptr_q;
  logic [PTR_W-1:0]   rd_ptr_q;
  logic [LEVEL_W-1:0] level_q;

  logic push_fire;
  logic pop_fire;

`ifndef SYNTHESIS
  localparam string RTL_DBG_TIMEUNIT_STR = RTL_DBG_TIME_MS ? "ms" :
                                           RTL_DBG_TIME_US ? "us" :
                                           RTL_DBG_TIME_NS ? "ns" : "ticks";

  logic dbg_push_blocked_q;

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

  assign empty = (level_q == '0);
  assign full  = (int'(level_q) == DEPTH);

  assign pop_valid  = !empty;
  assign pop_fire   = pop_valid && pop_ready;
  assign push_ready = !full || pop_fire;
  assign push_fire  = push_valid && push_ready;

  assign pop_data = mem_q[rd_ptr_q];
  assign level    = level_q;

  always_ff @(posedge clk) begin
    if (!rst_n) begin
      wr_ptr_q <= '0;
      rd_ptr_q <= '0;
      level_q  <= '0;
    end else if (clear) begin
      wr_ptr_q <= '0;
      rd_ptr_q <= '0;
      level_q  <= '0;
    end else begin
      if (push_fire) begin
        mem_q[wr_ptr_q] <= push_data;

        if (int'(wr_ptr_q) == (DEPTH - 1)) begin
          wr_ptr_q <= '0;
        end else begin
          wr_ptr_q <= wr_ptr_q + 1'b1;
        end
      end

      if (pop_fire) begin
        if (int'(rd_ptr_q) == (DEPTH - 1)) begin
          rd_ptr_q <= '0;
        end else begin
          rd_ptr_q <= rd_ptr_q + 1'b1;
        end
      end

      case ({push_fire, pop_fire})
        2'b10: begin
          level_q <= level_q + 1'b1;
        end

        2'b01: begin
          level_q <= level_q - 1'b1;
        end

        default: begin
          level_q <= level_q;
        end
      endcase
    end
  end

`ifndef SYNTHESIS
  always_ff @(posedge clk) begin
    if (!rst_n) begin
      dbg_push_blocked_q <= 1'b0;
    end else begin
      if (RTL_DBG) begin
        if (clear) begin
          `DPRINT($display(
            "[RTL][INFO][%0.0f %s] %m: FIFO clear, level_before=%0d",
            rtl_dbg_time(),
            RTL_DBG_TIMEUNIT_STR,
            level_q
          ));
        end else begin
          if (push_valid && !push_ready && !dbg_push_blocked_q) begin
            `DPRINT($display(
              "[RTL][WARN][%0.0f %s] %m: FIFO push blocked, level=%0d depth=%0d data=0x%0h",
              rtl_dbg_time(),
              RTL_DBG_TIMEUNIT_STR,
              level_q,
              DEPTH,
              push_data
            ));
          end

          if (!(push_valid && !push_ready) && dbg_push_blocked_q) begin
            `DPRINT($display(
              "[RTL][INFO][%0.0f %s] %m: FIFO push unblocked, level=%0d",
              rtl_dbg_time(),
              RTL_DBG_TIMEUNIT_STR,
              level_q
            ));
          end

          if (push_fire && pop_fire) begin
            `DPRINT($display(
              "[RTL][INFO][%0.0f %s] %m: FIFO push/pop, level_before=%0d push_data=0x%0h pop_data=0x%0h",
              rtl_dbg_time(),
              RTL_DBG_TIMEUNIT_STR,
              level_q,
              push_data,
              pop_data
            ));
          end else if (push_fire) begin
            `DPRINT($display(
              "[RTL][INFO][%0.0f %s] %m: FIFO push, level_before=%0d data=0x%0h",
              rtl_dbg_time(),
              RTL_DBG_TIMEUNIT_STR,
              level_q,
              push_data
            ));
          end else if (pop_fire) begin
            `DPRINT($display(
              "[RTL][INFO][%0.0f %s] %m: FIFO pop, level_before=%0d data=0x%0h",
              rtl_dbg_time(),
              RTL_DBG_TIMEUNIT_STR,
              level_q,
              pop_data
            ));
          end
        end
      end

      dbg_push_blocked_q <= push_valid && !push_ready;
    end
  end
`endif

endmodule

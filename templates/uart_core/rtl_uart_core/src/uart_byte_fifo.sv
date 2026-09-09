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
  parameter bit RTL_DBG = 1'b1
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
  logic dbg_push_blocked_q;
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
            "[RTL][INFO][%0t] %m: FIFO clear, level_before=%0d",
            $time,
            level_q
          ));
        end else begin
          if (push_valid && !push_ready && !dbg_push_blocked_q) begin
            `DPRINT($display(
              "[RTL][WARN][%0t] %m: FIFO push blocked, level=%0d depth=%0d data=0x%0h",
              $time,
              level_q,
              DEPTH,
              push_data
            ));
          end

          if (!(push_valid && !push_ready) && dbg_push_blocked_q) begin
            `DPRINT($display(
              "[RTL][INFO][%0t] %m: FIFO push unblocked, level=%0d",
              $time,
              level_q
            ));
          end

          if (push_fire && pop_fire) begin
            `DPRINT($display(
              "[RTL][INFO][%0t] %m: FIFO push/pop, level_before=%0d push_data=0x%0h pop_data=0x%0h",
              $time,
              level_q,
              push_data,
              pop_data
            ));
          end else if (push_fire) begin
            `DPRINT($display(
              "[RTL][INFO][%0t] %m: FIFO push, level_before=%0d data=0x%0h",
              $time,
              level_q,
              push_data
            ));
          end else if (pop_fire) begin
            `DPRINT($display(
              "[RTL][INFO][%0t] %m: FIFO pop, level_before=%0d data=0x%0h",
              $time,
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

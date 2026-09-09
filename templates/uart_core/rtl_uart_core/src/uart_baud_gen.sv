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

module uart_baud_gen #(
  parameter int unsigned BAUD_ACC_W = 32,
  parameter int unsigned OVERSAMPLE = 16,
  // ================================================================
  // DEBUG CONTROL
  // ================================================================
  parameter bit RTL_DBG = 1'b1
) (
  input  wire                  clk,
  input  wire                  rst_n,
  input  wire                  cfg_enable,
  input  wire [BAUD_ACC_W-1:0] cfg_baud_inc,
  output wire                  oversample_tick,
  output wire                  baud_tick
);

  localparam int unsigned OS_COUNT_W = (OVERSAMPLE <= 1) ? 1 : $clog2(OVERSAMPLE);

  localparam int unsigned OS_LAST_INT = (OVERSAMPLE <= 1) ? 0 : OVERSAMPLE - 1;

  logic [BAUD_ACC_W-1:0]  baud_acc_q;
  logic [BAUD_ACC_W:0]    baud_sum;
  logic [OS_COUNT_W-1:0]  oversample_count_q;
  logic                   oversample_tick_q;
  logic                   baud_tick_q;

`ifndef SYNTHESIS
  logic                  dbg_cfg_enable_q;
  logic                  dbg_seen_enable_q;
  logic [BAUD_ACC_W-1:0] dbg_baud_inc_q;
`endif

  assign baud_sum = {1'b0, baud_acc_q} + {1'b0, cfg_baud_inc};

  always_ff @(posedge clk) begin
    if (!rst_n) begin
      baud_acc_q         <= '0;
      oversample_count_q <= '0;
      oversample_tick_q  <= 1'b0;
      baud_tick_q        <= 1'b0;
    end else begin
      oversample_tick_q <= 1'b0;
      baud_tick_q       <= 1'b0;

      if (!cfg_enable) begin
        baud_acc_q         <= '0;
        oversample_count_q <= '0;
      end else begin
        baud_acc_q <= baud_sum[BAUD_ACC_W-1:0];

        if (baud_sum[BAUD_ACC_W]) begin
          oversample_tick_q <= 1'b1;

          if (int'(oversample_count_q) == OS_LAST_INT) begin
            oversample_count_q <= '0;
            baud_tick_q        <= 1'b1;
          end else begin
            oversample_count_q <= oversample_count_q + 1'b1;
          end
        end
      end
    end
  end

`ifndef SYNTHESIS
  always_ff @(posedge clk) begin
    if (!rst_n) begin
      dbg_cfg_enable_q <= 1'b0;
      dbg_seen_enable_q <= 1'b0;
      dbg_baud_inc_q    <= '0;
    end else begin
      if (RTL_DBG) begin
        if (cfg_enable && !dbg_cfg_enable_q) begin
          `DPRINT($display(
            "[RTL][INFO][%0t] %m: baud generator enabled, cfg_baud_inc=%0d, oversample=%0d",
            $time,
            cfg_baud_inc,
            OVERSAMPLE
          ));
        end

        if (!cfg_enable && dbg_cfg_enable_q) begin
          `DPRINT($display(
            "[RTL][INFO][%0t] %m: baud generator disabled and counters idled",
            $time
          ));
        end

        if (cfg_enable && dbg_seen_enable_q && (cfg_baud_inc != dbg_baud_inc_q)) begin
          `DPRINT($display(
            "[RTL][WARN][%0t] %m: cfg_baud_inc changed while enabled, old=%0d new=%0d",
            $time,
            dbg_baud_inc_q,
            cfg_baud_inc
          ));
        end
      end

      if (cfg_enable) begin
        dbg_seen_enable_q <= 1'b1;
      end

      dbg_cfg_enable_q <= cfg_enable;
      dbg_baud_inc_q    <= cfg_baud_inc;
    end
  end
`endif

  assign oversample_tick = oversample_tick_q;
  assign baud_tick       = baud_tick_q;

endmodule

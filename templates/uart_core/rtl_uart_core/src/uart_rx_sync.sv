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
  parameter bit RTL_DBG = 1'b1
) (
  input  wire clk,
  input  wire rst_n,
  input  wire uart_rx_i,
  output wire rx_sync_o
);

  localparam int unsigned SYNC_STAGES = 3;

  logic [SYNC_STAGES-1:0] rx_sync_q;

`ifndef SYNTHESIS
  logic dbg_activity_seen_q;
  logic dbg_rx_sync_d_q;
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
          "[RTL][INFO][%0t] %m: first synchronized RX activity observed, rx_sync_o=%0b",
          $time,
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

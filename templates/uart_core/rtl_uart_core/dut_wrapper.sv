`default_nettype none

// Simulation-only wrapper for uart_core.
//
// Purpose:
//   - Keep uart_core.sv synthesizable and unchanged.
//   - Generate DUT clocks natively in SystemVerilog instead of toggling every
//     clock edge through RapidVPI.
//   - Expose a small set of internal VPI-visible control/status signals so the
//     existing vip_common::Clock API remains transparent to all tc_* code.
//   - Keep one far-future native HDL timed event scheduled so Verilator's
//     stock --binary --vpi timing loop remains alive even when every DUT clock
//     is intentionally stopped by a testcase.
//
// -----------------------------------------------------------------------------
// USER CONTROLLABLE CLOCKS
// -----------------------------------------------------------------------------
//   UART core system clock -> clk
//     sim_clk_enable
//     sim_clk_period_ticks
//     sim_clk_stopped
//
// Control semantics:
//   *_enable        : 1 = run clock, 0 = stop and park clock low
//   *_period_ticks  : full clock period; one tick = 1ps in this wrapper
//   *_stopped       : status; 1 = clock is stopped and parked low
//
// The native clock generator clamps periods below 2 ticks to 2 ticks and uses:
//   high_ticks = period_ticks / 2
//   low_ticks  = period_ticks - high_ticks
//
// For deterministic start_at() semantics, vip_common programs the period
// first, then schedules the corresponding *_enable 0->1 write at the requested
// first-rise tick. The native generator produces the first rising edge in that
// same simulation time slot.
// -----------------------------------------------------------------------------

module sim_native_clock_gen (
    input  wire         enable_i,         // 1: run clock, 0: stop and park low
    input  wire  [63:0] period_ticks_i,   // Full clock period in simulation ticks
    output logic        clk_o = 1'b0,     // Generated clock output
    output logic        stopped_o = 1'b1  // 1 when clock is stopped and parked low
);

  timeunit 1ns; timeprecision 1ps;

  longint unsigned period_ticks_now;
  longint unsigned high_ticks;
  longint unsigned low_ticks;

  // Simulation-only clock process.
  //
  // Stop behavior intentionally takes effect on the next half-cycle boundary,
  // matching the existing RapidVPI clock agent's apply_requests_() behavior.
  // The clock is parked low before stopped_o is asserted.
  always begin : p_native_clock
    wait (enable_i === 1'b1);

    stopped_o = 1'b0;
    clk_o = 1'b1;

    while (enable_i === 1'b1) begin
      period_ticks_now = (period_ticks_i < 64'd2) ? 64'd2 : period_ticks_i;
      high_ticks = period_ticks_now / 2;
      low_ticks = period_ticks_now - high_ticks;

      #(high_ticks * 1ps);
      if (enable_i !== 1'b1) begin
        break;
      end

      clk_o = 1'b0;

      #(low_ticks * 1ps);
      if (enable_i !== 1'b1) begin
        break;
      end

      clk_o = 1'b1;
    end

    clk_o = 1'b0;
    stopped_o = 1'b1;
  end

endmodule

module dut_wrapper #(
    parameter int unsigned BAUD_ACC_W         = 32,
    parameter int unsigned OVERSAMPLE         = 16,
    parameter int unsigned RX_FIFO_DEPTH      = 16,
    parameter int unsigned TX_FIFO_DEPTH      = 16,
    parameter bit          HAS_RTS_CTS        = 1'b0,
    parameter bit          RTS_ACTIVE_LOW     = 1'b1,
    parameter bit          CTS_ACTIVE_LOW     = 1'b1,
    parameter int unsigned RTS_DEASSERT_LEVEL = RX_FIFO_DEPTH - 2,
    parameter int unsigned RTS_ASSERT_LEVEL   = RX_FIFO_DEPTH / 2,
    parameter bit          RTL_DBG            = 1'b1
) (
    input  wire                                      rst_n,

    input  wire                                      uart_rx_i,
    output wire                                      uart_tx_o,
    input  wire                                      uart_cts_i,
    output wire                                      uart_rts_o,

    input  wire                                      cfg_enable,
    input  wire                                      cfg_rx_enable,
    input  wire                                      cfg_tx_enable,
    input  wire [BAUD_ACC_W-1:0]                     cfg_baud_inc,
    input  wire [1:0]                                cfg_parity_mode,
    input  wire [1:0]                                cfg_stop_bits,
    input  wire [1:0]                                cfg_data_bits,
    input  wire                                      cfg_hw_flow_enable,
    input  wire                                      ctrl_rx_fifo_clear,
    input  wire                                      ctrl_tx_fifo_clear,

    input  wire                                      tx_byte_valid,
    output wire                                      tx_byte_ready,
    input  wire [7:0]                                tx_byte_data,

    output wire                                      rx_byte_valid,
    input  wire                                      rx_byte_ready,
    output wire [7:0]                                rx_byte_data,

    output wire                                      rx_byte_frame_error,
    output wire                                      rx_byte_parity_error,
    output wire                                      rx_byte_break_detect,

    output wire [$clog2(RX_FIFO_DEPTH + 1)-1:0]      rx_fifo_level,
    output wire [$clog2(TX_FIFO_DEPTH + 1)-1:0]      tx_fifo_level,
    output wire                                      rx_fifo_empty,
    output wire                                      rx_fifo_full,
    output wire                                      tx_fifo_empty,
    output wire                                      tx_fifo_full,

    output wire                                      rx_busy,
    output wire                                      tx_busy,
    output wire                                      cts_active,
    output wire                                      rts_active,
    output wire                                      cts_blocked,

    output wire                                      event_rx_overrun,
    output wire                                      event_rx_frame_error,
    output wire                                      event_rx_parity_error,
    output wire                                      event_rx_break_detect,
    output wire                                      event_tx_done
);

  timeunit 1ns; timeprecision 1ps;

  // --------------------------------------------------------------------------
  // USER CONTROLLABLE CLOCKS
  // --------------------------------------------------------------------------
  logic        sim_clk_enable /* verilator public_flat_rw */ = 1'b0;
  logic [63:0] sim_clk_period_ticks /* verilator public_flat_rw */ = 64'd10000;
  logic        sim_clk_stopped /* verilator public_flat_rd */;

  // --------------------------------------------------------------------------
  // Generated DUT clock nets
  // --------------------------------------------------------------------------
  logic clk /* verilator public_flat_rd */;

  // --------------------------------------------------------------------------
  // Native simulation clocks
  // --------------------------------------------------------------------------
  sim_native_clock_gen u_sim_clk (
      .enable_i      (sim_clk_enable),
      .period_ticks_i(sim_clk_period_ticks),
      .clk_o         (clk),
      .stopped_o     (sim_clk_stopped)
  );

  // --------------------------------------------------------------------------
  // Simulation timing-queue keepalive for Verilator
  // --------------------------------------------------------------------------
  // Far-future native timed event so Verilator does not terminate merely
  // because all controllable DUT clocks are intentionally stopped.
  logic sim_keepalive /* verilator public_flat_rd */ = 1'b0;

  always begin : p_sim_keepalive
    #1s;
    sim_keepalive = ~sim_keepalive;
  end

  // --------------------------------------------------------------------------
  // Synthesizable DUT instance
  // --------------------------------------------------------------------------
  uart_core #(
      .BAUD_ACC_W        (BAUD_ACC_W),
      .OVERSAMPLE        (OVERSAMPLE),
      .RX_FIFO_DEPTH     (RX_FIFO_DEPTH),
      .TX_FIFO_DEPTH     (TX_FIFO_DEPTH),
      .HAS_RTS_CTS       (HAS_RTS_CTS),
      .RTS_ACTIVE_LOW    (RTS_ACTIVE_LOW),
      .CTS_ACTIVE_LOW    (CTS_ACTIVE_LOW),
      .RTS_DEASSERT_LEVEL(RTS_DEASSERT_LEVEL),
      .RTS_ASSERT_LEVEL  (RTS_ASSERT_LEVEL),
      .RTL_DBG           (RTL_DBG)
  ) u_dut (
      .clk                  (clk),
      .rst_n                (rst_n),
      .uart_rx_i            (uart_rx_i),
      .uart_tx_o            (uart_tx_o),
      .uart_cts_i           (uart_cts_i),
      .uart_rts_o           (uart_rts_o),
      .cfg_enable           (cfg_enable),
      .cfg_rx_enable        (cfg_rx_enable),
      .cfg_tx_enable        (cfg_tx_enable),
      .cfg_baud_inc         (cfg_baud_inc),
      .cfg_parity_mode      (cfg_parity_mode),
      .cfg_stop_bits        (cfg_stop_bits),
      .cfg_data_bits        (cfg_data_bits),
      .cfg_hw_flow_enable   (cfg_hw_flow_enable),
      .ctrl_rx_fifo_clear   (ctrl_rx_fifo_clear),
      .ctrl_tx_fifo_clear   (ctrl_tx_fifo_clear),
      .tx_byte_valid        (tx_byte_valid),
      .tx_byte_ready        (tx_byte_ready),
      .tx_byte_data         (tx_byte_data),
      .rx_byte_valid        (rx_byte_valid),
      .rx_byte_ready        (rx_byte_ready),
      .rx_byte_data         (rx_byte_data),
      .rx_byte_frame_error  (rx_byte_frame_error),
      .rx_byte_parity_error (rx_byte_parity_error),
      .rx_byte_break_detect (rx_byte_break_detect),
      .rx_fifo_level        (rx_fifo_level),
      .tx_fifo_level        (tx_fifo_level),
      .rx_fifo_empty        (rx_fifo_empty),
      .rx_fifo_full         (rx_fifo_full),
      .tx_fifo_empty        (tx_fifo_empty),
      .tx_fifo_full         (tx_fifo_full),
      .rx_busy              (rx_busy),
      .tx_busy              (tx_busy),
      .cts_active           (cts_active),
      .rts_active           (rts_active),
      .cts_blocked          (cts_blocked),
      .event_rx_overrun     (event_rx_overrun),
      .event_rx_frame_error (event_rx_frame_error),
      .event_rx_parity_error(event_rx_parity_error),
      .event_rx_break_detect(event_rx_break_detect),
      .event_tx_done        (event_tx_done)
  );

endmodule

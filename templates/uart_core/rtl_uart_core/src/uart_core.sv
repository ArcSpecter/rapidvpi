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

module uart_core #(
    parameter int unsigned BAUD_ACC_W         = 32, // cfg_baud_inc = round(baud_rate * OVERSAMPLE * 2^BAUD_ACC_W / clk_hz)
    parameter int unsigned OVERSAMPLE = 16,
    parameter int unsigned RX_FIFO_DEPTH = 16,
    parameter int unsigned TX_FIFO_DEPTH = 16,
    parameter bit HAS_RTS_CTS = 1'b0,
    parameter bit RTS_ACTIVE_LOW = 1'b1,
    parameter bit CTS_ACTIVE_LOW = 1'b1,
    parameter int unsigned RTS_DEASSERT_LEVEL = RX_FIFO_DEPTH - 2,
    parameter int unsigned RTS_ASSERT_LEVEL = RX_FIFO_DEPTH / 2,
    // ================================================================
    // DEBUG CONTROL
    // ================================================================
    parameter bit RTL_DBG = 1'b1,
    parameter bit RTL_DBG_TIME_NS = 1'b1,
    parameter bit RTL_DBG_TIME_US = 1'b0,
    parameter bit RTL_DBG_TIME_MS = 1'b0
) (
    input wire clk,
    input wire rst_n,

    input  wire uart_rx_i,
    output wire uart_tx_o,
    input  wire uart_cts_i,
    output wire uart_rts_o,

    input wire                  cfg_enable,          // Global core enable
    input wire                  cfg_rx_enable,       // Enables RX engine to receive new characters
    input wire                  cfg_tx_enable,       // Enables TX engine to launch new characters
    input wire [BAUD_ACC_W-1:0] cfg_baud_inc,        // Fractional/NCO baud increment
    input wire [           1:0] cfg_parity_mode,     // Parity mode; defined in uart_pkg.sv
    input wire [           1:0] cfg_stop_bits,       // Stop-bit mode; defined in uart_pkg.sv
    input wire [           1:0] cfg_data_bits,       // Data-bit mode; defined in uart_pkg.sv
    input wire                  cfg_hw_flow_enable,  // Runtime RTS/CTS flow-control enable
    input wire                  ctrl_rx_fifo_clear,  // One-cycle pulse to clear RX FIFO records
    input wire                  ctrl_tx_fifo_clear,  // One-cycle pulse to clear queued TX bytes

    input  wire       tx_byte_valid,  // Upper layer presents one TX byte
    output wire       tx_byte_ready,  // Core can accept one TX byte into TX FIFO
    input  wire [7:0] tx_byte_data,   // TX byte payload from upper layer

    output wire       rx_byte_valid,  // Core presents one RX FIFO record
    input  wire       rx_byte_ready,  // Upper layer accepts/pops current RX record
    output wire [7:0] rx_byte_data,   // RX byte payload from current RX record

    output wire rx_byte_frame_error,   // Frame-error metadata for current RX record
    output wire rx_byte_parity_error,  // Parity-error metadata for current RX record
    output wire rx_byte_break_detect,  // Break-detect metadata for current RX record

    output wire [$clog2(RX_FIFO_DEPTH + 1)-1:0] rx_fifo_level,  // Number of stored RX records
    output wire [$clog2(TX_FIFO_DEPTH + 1)-1:0] tx_fifo_level,  // Number of queued TX bytes
    output wire rx_fifo_empty,  // RX FIFO has no stored records
    output wire rx_fifo_full,  // RX FIFO cannot accept another record
    output wire tx_fifo_empty,  // TX FIFO has no queued bytes
    output wire tx_fifo_full,  // TX FIFO cannot accept another byte

    output wire rx_busy,     // RX engine is receiving a character
    output wire tx_busy,     // TX engine is serializing a character
    output wire cts_active,  // Normalized CTS state; peer permits transmit
    output wire rts_active,  // Normalized RTS state; core permits peer transmit
    output wire cts_blocked, // TX launch is blocked by inactive CTS

    output wire event_rx_overrun,       // Pulse when RX record is dropped due to full FIFO
    output wire event_rx_frame_error,   // Pulse when RX frame error is detected
    output wire event_rx_parity_error,  // Pulse when RX parity error is detected
    output wire event_rx_break_detect,  // Pulse when RX break condition is detected
    output wire event_tx_done           // Pulse when one TX character fully completes
);

  localparam int unsigned RX_LEVEL_W = $clog2(RX_FIFO_DEPTH + 1);
  localparam int unsigned TX_LEVEL_W = $clog2(TX_FIFO_DEPTH + 1);
  localparam int unsigned RX_FIFO_DATA_W = 11;
  localparam int unsigned TX_FIFO_DATA_W = 8;

  logic                      rx_sync;
  logic                      oversample_tick;
  logic                      baud_tick;

  logic                      rx_char_valid;
  logic [               7:0] rx_char_data;
  logic                      rx_char_frame_error;
  logic                      rx_char_parity_error;
  logic                      rx_char_break_detect;

  logic                      rx_fifo_push_valid;
  logic                      rx_fifo_push_ready;
  logic [RX_FIFO_DATA_W-1:0] rx_fifo_push_data;
  logic                      rx_fifo_pop_valid;
  logic [RX_FIFO_DATA_W-1:0] rx_fifo_pop_data;
  logic [    RX_LEVEL_W-1:0] rx_fifo_level_w;
  logic                      rx_fifo_empty_w;
  logic                      rx_fifo_full_w;

  logic                      tx_fifo_push_valid;
  logic                      tx_fifo_push_ready;
  logic                      tx_fifo_pop_ready;
  logic                      tx_fifo_pop_valid;
  logic [TX_FIFO_DATA_W-1:0] tx_fifo_pop_data;
  logic [    TX_LEVEL_W-1:0] tx_fifo_level_w;
  logic                      tx_fifo_empty_w;
  logic                      tx_fifo_full_w;

  logic                      tx_ready_for_start;
  logic                      tx_launch_wanted;
  logic                      tx_start_allowed;
  logic                      tx_start;
  logic                      flow_uart_rts;
  logic                      flow_cts_active;
  logic                      flow_rts_active;
  logic                      flow_cts_blocked;

`ifndef SYNTHESIS
  localparam string RTL_DBG_TIMEUNIT_STR = RTL_DBG_TIME_MS ? "ms" :
                                           RTL_DBG_TIME_US ? "us" :
                                           RTL_DBG_TIME_NS ? "ns" : "ticks";

  logic dbg_reset_seen_q;
  logic dbg_cfg_enable_q;
  logic dbg_cfg_rx_enable_q;
  logic dbg_cfg_tx_enable_q;
  logic dbg_cfg_hw_flow_enable_q;
  logic dbg_cts_blocked_q;

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

  uart_rx_sync #(
      .RTL_DBG        (RTL_DBG),
      .RTL_DBG_TIME_NS(RTL_DBG_TIME_NS),
      .RTL_DBG_TIME_US(RTL_DBG_TIME_US),
      .RTL_DBG_TIME_MS(RTL_DBG_TIME_MS)
  ) u_rx_sync (
      .clk      (clk),
      .rst_n    (rst_n),
      .uart_rx_i(uart_rx_i),
      .rx_sync_o(rx_sync)
  );

  uart_baud_gen #(
      .BAUD_ACC_W     (BAUD_ACC_W),
      .OVERSAMPLE     (OVERSAMPLE),
      .RTL_DBG        (RTL_DBG),
      .RTL_DBG_TIME_NS(RTL_DBG_TIME_NS),
      .RTL_DBG_TIME_US(RTL_DBG_TIME_US),
      .RTL_DBG_TIME_MS(RTL_DBG_TIME_MS)
  ) u_baud_gen (
      .clk            (clk),
      .rst_n          (rst_n),
      .cfg_enable     (cfg_enable),
      .cfg_baud_inc   (cfg_baud_inc),
      .oversample_tick(oversample_tick),
      .baud_tick      (baud_tick)
  );

  uart_rx #(
      .OVERSAMPLE     (OVERSAMPLE),
      .RTL_DBG        (RTL_DBG),
      .RTL_DBG_TIME_NS(RTL_DBG_TIME_NS),
      .RTL_DBG_TIME_US(RTL_DBG_TIME_US),
      .RTL_DBG_TIME_MS(RTL_DBG_TIME_MS)
  ) u_rx (
      .clk                  (clk),
      .rst_n                (rst_n),
      .cfg_enable           (cfg_enable),
      .cfg_rx_enable        (cfg_rx_enable),
      .cfg_parity_mode      (cfg_parity_mode),
      .cfg_stop_bits        (cfg_stop_bits),
      .cfg_data_bits        (cfg_data_bits),
      .oversample_tick      (oversample_tick),
      .rx_sync_i            (rx_sync),
      .rx_char_valid        (rx_char_valid),
      .rx_char_data         (rx_char_data),
      .rx_char_frame_error  (rx_char_frame_error),
      .rx_char_parity_error (rx_char_parity_error),
      .rx_char_break_detect (rx_char_break_detect),
      .rx_busy              (rx_busy),
      .event_rx_frame_error (event_rx_frame_error),
      .event_rx_parity_error(event_rx_parity_error),
      .event_rx_break_detect(event_rx_break_detect)
  );

  assign rx_fifo_push_valid = rx_char_valid && !ctrl_rx_fifo_clear;
  assign rx_fifo_push_data = {
    rx_char_break_detect, rx_char_parity_error, rx_char_frame_error, rx_char_data
  };

  uart_byte_fifo #(
      .DATA_W         (RX_FIFO_DATA_W),
      .DEPTH          (RX_FIFO_DEPTH),
      .RTL_DBG        (RTL_DBG),
      .RTL_DBG_TIME_NS(RTL_DBG_TIME_NS),
      .RTL_DBG_TIME_US(RTL_DBG_TIME_US),
      .RTL_DBG_TIME_MS(RTL_DBG_TIME_MS)
  ) u_rx_fifo (
      .clk       (clk),
      .rst_n     (rst_n),
      .clear     (ctrl_rx_fifo_clear),
      .push_valid(rx_fifo_push_valid),
      .push_ready(rx_fifo_push_ready),
      .push_data (rx_fifo_push_data),
      .pop_valid (rx_fifo_pop_valid),
      .pop_ready (rx_byte_ready),
      .pop_data  (rx_fifo_pop_data),
      .level     (rx_fifo_level_w),
      .empty     (rx_fifo_empty_w),
      .full      (rx_fifo_full_w)
  );

  assign event_rx_overrun     = rx_char_valid && !ctrl_rx_fifo_clear && !rx_fifo_push_ready;
  assign rx_byte_valid        = rx_fifo_pop_valid;
  assign rx_byte_data         = rx_fifo_pop_data[7:0];
  assign rx_byte_frame_error  = rx_fifo_pop_data[8];
  assign rx_byte_parity_error = rx_fifo_pop_data[9];
  assign rx_byte_break_detect = rx_fifo_pop_data[10];
  assign rx_fifo_level        = rx_fifo_level_w;
  assign rx_fifo_empty        = rx_fifo_empty_w;
  assign rx_fifo_full         = rx_fifo_full_w;

  assign tx_fifo_push_valid   = tx_byte_valid && !ctrl_tx_fifo_clear;
  assign tx_byte_ready        = tx_fifo_push_ready && !ctrl_tx_fifo_clear;

  uart_byte_fifo #(
      .DATA_W         (TX_FIFO_DATA_W),
      .DEPTH          (TX_FIFO_DEPTH),
      .RTL_DBG        (RTL_DBG),
      .RTL_DBG_TIME_NS(RTL_DBG_TIME_NS),
      .RTL_DBG_TIME_US(RTL_DBG_TIME_US),
      .RTL_DBG_TIME_MS(RTL_DBG_TIME_MS)
  ) u_tx_fifo (
      .clk       (clk),
      .rst_n     (rst_n),
      .clear     (ctrl_tx_fifo_clear),
      .push_valid(tx_fifo_push_valid),
      .push_ready(tx_fifo_push_ready),
      .push_data (tx_byte_data),
      .pop_valid (tx_fifo_pop_valid),
      .pop_ready (tx_fifo_pop_ready),
      .pop_data  (tx_fifo_pop_data),
      .level     (tx_fifo_level_w),
      .empty     (tx_fifo_empty_w),
      .full      (tx_fifo_full_w)
  );

  assign tx_launch_wanted  = cfg_enable && cfg_tx_enable && tx_fifo_pop_valid && tx_ready_for_start;
  assign tx_start          = tx_launch_wanted && tx_start_allowed && baud_tick;
  assign tx_fifo_pop_ready = tx_start;

  uart_tx #(
      .RTL_DBG        (RTL_DBG),
      .RTL_DBG_TIME_NS(RTL_DBG_TIME_NS),
      .RTL_DBG_TIME_US(RTL_DBG_TIME_US),
      .RTL_DBG_TIME_MS(RTL_DBG_TIME_MS)
  ) u_tx (
      .clk               (clk),
      .rst_n             (rst_n),
      .cfg_enable        (cfg_enable),
      .cfg_tx_enable     (cfg_tx_enable),
      .cfg_parity_mode   (cfg_parity_mode),
      .cfg_stop_bits     (cfg_stop_bits),
      .cfg_data_bits     (cfg_data_bits),
      .baud_tick         (baud_tick),
      .tx_start          (tx_start),
      .tx_data           (tx_fifo_pop_data),
      .tx_ready_for_start(tx_ready_for_start),
      .tx_busy           (tx_busy),
      .uart_tx_o         (uart_tx_o),
      .event_tx_done     (event_tx_done)
  );

  generate
    if (HAS_RTS_CTS) begin : g_hw_flow_ctrl
      uart_hw_flow_ctrl #(
          .RX_LEVEL_W        (RX_LEVEL_W),
          .RTS_ACTIVE_LOW    (RTS_ACTIVE_LOW),
          .CTS_ACTIVE_LOW    (CTS_ACTIVE_LOW),
          .RTS_DEASSERT_LEVEL(RTS_DEASSERT_LEVEL),
          .RTS_ASSERT_LEVEL  (RTS_ASSERT_LEVEL),
          .RTL_DBG           (RTL_DBG),
          .RTL_DBG_TIME_NS   (RTL_DBG_TIME_NS),
          .RTL_DBG_TIME_US   (RTL_DBG_TIME_US),
          .RTL_DBG_TIME_MS   (RTL_DBG_TIME_MS)
      ) u_hw_flow_ctrl (
          .clk               (clk),
          .rst_n             (rst_n),
          .cfg_enable        (cfg_enable),
          .cfg_rx_enable     (cfg_rx_enable),
          .cfg_hw_flow_enable(cfg_hw_flow_enable),
          .uart_cts_i        (uart_cts_i),
          .rx_fifo_level     (rx_fifo_level_w),
          .tx_launch_wanted  (tx_launch_wanted),
          .tx_start_allowed  (tx_start_allowed),
          .uart_rts_o        (flow_uart_rts),
          .cts_active        (flow_cts_active),
          .rts_active        (flow_rts_active),
          .cts_blocked       (flow_cts_blocked)
      );
    end else begin : g_no_hw_flow_ctrl
      assign tx_start_allowed = 1'b1;
      assign flow_uart_rts    = RTS_ACTIVE_LOW ? 1'b1 : 1'b0;
      assign flow_cts_active  = 1'b1;
      assign flow_rts_active  = 1'b0;
      assign flow_cts_blocked = 1'b0;
    end
  endgenerate

  assign uart_rts_o  = flow_uart_rts;
  assign cts_active  = flow_cts_active;
  assign rts_active  = flow_rts_active;
  assign cts_blocked = flow_cts_blocked;

`ifndef SYNTHESIS
  always_ff @(posedge clk) begin
    if (!rst_n) begin
      dbg_reset_seen_q         <= 1'b0;
      dbg_cfg_enable_q         <= 1'b0;
      dbg_cfg_rx_enable_q      <= 1'b0;
      dbg_cfg_tx_enable_q      <= 1'b0;
      dbg_cfg_hw_flow_enable_q <= 1'b0;
      dbg_cts_blocked_q        <= 1'b0;
    end else begin
      if (RTL_DBG) begin
        if (!dbg_reset_seen_q) begin
          `DPRINT($display(
                  "[RTL][INFO][%0.0f %s] %m: UART core debug enabled, baud_acc_w=%0d oversample=%0d rx_fifo_depth=%0d tx_fifo_depth=%0d has_rts_cts=%0b",
                  rtl_dbg_time(),
                  RTL_DBG_TIMEUNIT_STR,
                  BAUD_ACC_W,
                  OVERSAMPLE,
                  RX_FIFO_DEPTH,
                  TX_FIFO_DEPTH,
                  HAS_RTS_CTS
                  ));
        end

        if ((cfg_enable != dbg_cfg_enable_q) ||
            (cfg_rx_enable != dbg_cfg_rx_enable_q) ||
            (cfg_tx_enable != dbg_cfg_tx_enable_q) ||
            (cfg_hw_flow_enable != dbg_cfg_hw_flow_enable_q)) begin
          `DPRINT($display(
                  "[RTL][INFO][%0.0f %s] %m: config enables changed, core=%0b rx=%0b tx=%0b hw_flow=%0b",
                  rtl_dbg_time(),
                  RTL_DBG_TIMEUNIT_STR,
                  cfg_enable,
                  cfg_rx_enable,
                  cfg_tx_enable,
                  cfg_hw_flow_enable
                  ));
        end

        if (ctrl_rx_fifo_clear) begin
          `DPRINT($display(
                  "[RTL][INFO][%0.0f %s] %m: RX FIFO clear requested, level_before=%0d",
                  rtl_dbg_time(),
                  RTL_DBG_TIMEUNIT_STR,
                  rx_fifo_level_w
                  ));
        end

        if (ctrl_tx_fifo_clear) begin
          `DPRINT($display(
                  "[RTL][INFO][%0.0f %s] %m: TX FIFO clear requested, level_before=%0d",
                  rtl_dbg_time(),
                  RTL_DBG_TIMEUNIT_STR,
                  tx_fifo_level_w
                  ));
        end

        if (rx_fifo_push_valid && rx_fifo_push_ready) begin
          `DPRINT($display(
                  "[RTL][INFO][%0.0f %s] %m: RX record queued, data=0x%02h frame=%0b parity=%0b break=%0b rx_level_before=%0d",
                  rtl_dbg_time(),
                  RTL_DBG_TIMEUNIT_STR,
                  rx_char_data,
                  rx_char_frame_error,
                  rx_char_parity_error,
                  rx_char_break_detect,
                  rx_fifo_level_w
                  ));
        end

        if (event_rx_overrun) begin
          `DPRINT($display(
                  "[RTL][WARN][%0.0f %s] %m: RX overrun, dropping data=0x%02h frame=%0b parity=%0b break=%0b rx_level=%0d",
                  rtl_dbg_time(),
                  RTL_DBG_TIMEUNIT_STR,
                  rx_char_data,
                  rx_char_frame_error,
                  rx_char_parity_error,
                  rx_char_break_detect,
                  rx_fifo_level_w
                  ));
        end

        if (rx_byte_valid && rx_byte_ready) begin
          `DPRINT($display(
                  "[RTL][INFO][%0.0f %s] %m: RX record consumed, data=0x%02h frame=%0b parity=%0b break=%0b rx_level_before=%0d",
                  rtl_dbg_time(),
                  RTL_DBG_TIMEUNIT_STR,
                  rx_byte_data,
                  rx_byte_frame_error,
                  rx_byte_parity_error,
                  rx_byte_break_detect,
                  rx_fifo_level_w
                  ));
        end

        if (tx_start) begin
          `DPRINT($display(
                  "[RTL][INFO][%0.0f %s] %m: TX launch from FIFO, data=0x%02h tx_level_before=%0d cts_active=%0b",
                  rtl_dbg_time(),
                  RTL_DBG_TIMEUNIT_STR,
                  tx_fifo_pop_data,
                  tx_fifo_level_w,
                  flow_cts_active
                  ));
        end

        if (flow_cts_blocked && !dbg_cts_blocked_q) begin
          `DPRINT($display(
                  "[RTL][WARN][%0.0f %s] %m: TX launch pending but blocked by CTS, tx_level=%0d",
                  rtl_dbg_time(),
                  RTL_DBG_TIMEUNIT_STR,
                  tx_fifo_level_w
                  ));
        end
      end

      dbg_reset_seen_q         <= 1'b1;
      dbg_cfg_enable_q         <= cfg_enable;
      dbg_cfg_rx_enable_q      <= cfg_rx_enable;
      dbg_cfg_tx_enable_q      <= cfg_tx_enable;
      dbg_cfg_hw_flow_enable_q <= cfg_hw_flow_enable;
      dbg_cts_blocked_q        <= flow_cts_blocked;
    end
  end
`endif

  assign tx_fifo_level = tx_fifo_level_w;
  assign tx_fifo_empty = tx_fifo_empty_w;
  assign tx_fifo_full  = tx_fifo_full_w;

endmodule

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

module uart_rx #(
  parameter int unsigned OVERSAMPLE = 16,
  // ================================================================
  // DEBUG CONTROL
  // ================================================================
  parameter bit RTL_DBG         = 1'b0,
  parameter bit RTL_DBG_TIME_NS = 1'b1,
  parameter bit RTL_DBG_TIME_US = 1'b0,
  parameter bit RTL_DBG_TIME_MS = 1'b0
) (
  input  wire       clk,
  input  wire       rst_n,
  input  wire       cfg_enable,
  input  wire       cfg_rx_enable,
  input  wire [1:0] cfg_parity_mode,
  input  wire [1:0] cfg_stop_bits,
  input  wire [1:0] cfg_data_bits,
  input  wire       oversample_tick,
  input  wire       rx_sync_i,
  output wire       rx_char_valid,
  output wire [7:0] rx_char_data,
  output wire       rx_char_frame_error,
  output wire       rx_char_parity_error,
  output wire       rx_char_break_detect,
  output wire       rx_busy,
  output wire       event_rx_frame_error,
  output wire       event_rx_parity_error,
  output wire       event_rx_break_detect
);

  import uart_pkg::*;

  typedef enum logic [2:0] {
    S_IDLE,
    S_START,
    S_DATA,
    S_PARITY,
    S_STOP,
    S_DONE
  } uart_rx_state_t;

  localparam int unsigned OS_COUNT_W       = (OVERSAMPLE <= 1) ? 1 : $clog2(OVERSAMPLE);
  localparam int unsigned BIT_LAST_INT     = (OVERSAMPLE <= 1) ? 0 : OVERSAMPLE - 1;
  localparam int unsigned HALF_LAST_INT    = (OVERSAMPLE <= 2) ? 0 : (OVERSAMPLE / 2) - 1;

  uart_rx_state_t s_current;

  logic [OS_COUNT_W-1:0] sample_count_q;
  logic [3:0]            data_bit_index_q;
  logic [3:0]            stop_bit_index_q;
  logic [3:0]            active_data_bits_q;
  logic [3:0]            active_stop_bits_q;
  logic [1:0]            active_parity_mode_q;
  logic [7:0]            data_q;
  logic                  all_data_zero_q;
  logic                  stop_low_seen_q;
  logic                  frame_error_q;
  logic                  parity_error_q;
  logic                  break_detect_q;
  logic                  start_armed_q;
  logic                  rx_char_valid_q;
  logic                  event_rx_frame_error_q;
  logic                  event_rx_parity_error_q;
  logic                  event_rx_break_detect_q;

`ifndef SYNTHESIS
  localparam string RTL_DBG_TIMEUNIT_STR = RTL_DBG_TIME_MS ? "ms" :
                                           RTL_DBG_TIME_US ? "us" :
                                           RTL_DBG_TIME_NS ? "ns" : "ticks";

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

  function automatic logic [3:0] decode_data_bits(input logic [1:0] data_bits);
    begin
      case (data_bits)
        UART_DATA_5: decode_data_bits = 4'd5;
        UART_DATA_6: decode_data_bits = 4'd6;
        UART_DATA_7: decode_data_bits = 4'd7;
        UART_DATA_8: decode_data_bits = 4'd8;
        default:     decode_data_bits = 4'd8;
      endcase
    end
  endfunction

  function automatic logic [3:0] decode_stop_bits(input logic [1:0] stop_bits);
    begin
      case (stop_bits)
        UART_STOP_1: decode_stop_bits = 4'd1;
        UART_STOP_2: decode_stop_bits = 4'd2;
        UART_STOP_RSVD0,
        UART_STOP_RSVD1: decode_stop_bits = 4'd1;
        default:         decode_stop_bits = 4'd1;
      endcase
    end
  endfunction

  function automatic logic parity_is_enabled(input logic [1:0] parity_mode);
    begin
      case (parity_mode)
        UART_PARITY_EVEN,
        UART_PARITY_ODD: parity_is_enabled = 1'b1;
        UART_PARITY_NONE,
        UART_PARITY_RSVD: parity_is_enabled = 1'b0;
        default:          parity_is_enabled = 1'b0;
      endcase
    end
  endfunction

  function automatic logic calc_parity_error(
    input logic [7:0] data,
    input logic [1:0] parity_mode,
    input logic       parity_bit
  );
    begin
      case (parity_mode)
        UART_PARITY_EVEN: calc_parity_error = (^data) ^ parity_bit;
        UART_PARITY_ODD:  calc_parity_error = !((^data) ^ parity_bit);
        default:          calc_parity_error = 1'b0;
      endcase
    end
  endfunction

  always_ff @(posedge clk) begin
    if (!rst_n) begin
      s_current                <= S_IDLE;
      sample_count_q           <= '0;
      data_bit_index_q         <= '0;
      stop_bit_index_q         <= '0;
      active_data_bits_q       <= 4'd8;
      active_stop_bits_q       <= 4'd1;
      active_parity_mode_q     <= UART_PARITY_NONE;
      data_q                   <= 8'h00;
      all_data_zero_q          <= 1'b1;
      stop_low_seen_q          <= 1'b0;
      frame_error_q            <= 1'b0;
      parity_error_q           <= 1'b0;
      break_detect_q           <= 1'b0;
      start_armed_q            <= 1'b1;
      rx_char_valid_q          <= 1'b0;
      event_rx_frame_error_q   <= 1'b0;
      event_rx_parity_error_q  <= 1'b0;
      event_rx_break_detect_q  <= 1'b0;
    end else begin
      rx_char_valid_q         <= 1'b0;
      event_rx_frame_error_q  <= 1'b0;
      event_rx_parity_error_q <= 1'b0;
      event_rx_break_detect_q <= 1'b0;

      case (s_current)
        S_IDLE: begin
          sample_count_q       <= '0;
          data_bit_index_q     <= '0;
          stop_bit_index_q     <= '0;
          stop_low_seen_q      <= 1'b0;
          frame_error_q        <= 1'b0;
          parity_error_q       <= 1'b0;
          break_detect_q       <= 1'b0;

          if (rx_sync_i) begin
            start_armed_q <= 1'b1;
          end else if (start_armed_q) begin
            start_armed_q <= 1'b0;

            if (cfg_enable && cfg_rx_enable) begin
              s_current            <= S_START;
              active_data_bits_q   <= decode_data_bits(cfg_data_bits);
              active_stop_bits_q   <= decode_stop_bits(cfg_stop_bits);
              active_parity_mode_q <= cfg_parity_mode;
              data_q               <= 8'h00;
              all_data_zero_q      <= 1'b1;
`ifndef SYNTHESIS
              if (RTL_DBG) begin
                `DPRINT($display(
                  "[RTL][INFO][%0.0f %s] %m: RX start edge detected, data_bits=%0d stop_bits=%0d parity_mode=0x%0h",
                  rtl_dbg_time(),
                  RTL_DBG_TIMEUNIT_STR,
                  decode_data_bits(cfg_data_bits),
                  decode_stop_bits(cfg_stop_bits),
                  cfg_parity_mode
                ));
              end
`endif
            end
          end
        end

        S_START: begin
          if (oversample_tick) begin
            if (int'(sample_count_q) == HALF_LAST_INT) begin
              sample_count_q <= '0;

              if (!rx_sync_i) begin
                s_current <= S_DATA;
              end else begin
                s_current <= S_IDLE;
`ifndef SYNTHESIS
                if (RTL_DBG) begin
                  `DPRINT($display(
                    "[RTL][WARN][%0.0f %s] %m: false RX start, mid-start sample returned high",
                    rtl_dbg_time(),
                    RTL_DBG_TIMEUNIT_STR
                  ));
                end
`endif
              end
            end else begin
              sample_count_q <= sample_count_q + 1'b1;
            end
          end
        end

        S_DATA: begin
          if (oversample_tick) begin
            if (int'(sample_count_q) == BIT_LAST_INT) begin
              sample_count_q                  <= '0;
              data_q[data_bit_index_q[2:0]]   <= rx_sync_i;
              all_data_zero_q                 <= all_data_zero_q && !rx_sync_i;

              if (data_bit_index_q == (active_data_bits_q - 4'd1)) begin
                data_bit_index_q <= '0;

                if (parity_is_enabled(active_parity_mode_q)) begin
                  s_current <= S_PARITY;
                end else begin
                  s_current <= S_STOP;
                end
              end else begin
                data_bit_index_q <= data_bit_index_q + 4'd1;
              end
            end else begin
              sample_count_q <= sample_count_q + 1'b1;
            end
          end
        end

        S_PARITY: begin
          if (oversample_tick) begin
            if (int'(sample_count_q) == BIT_LAST_INT) begin
              sample_count_q <= '0;
              parity_error_q <= calc_parity_error(data_q,
                                                   active_parity_mode_q,
                                                   rx_sync_i);
`ifndef SYNTHESIS
              if (RTL_DBG &&
                  calc_parity_error(data_q, active_parity_mode_q, rx_sync_i)) begin
                `DPRINT($display(
                  "[RTL][WARN][%0.0f %s] %m: RX parity error, data=0x%02h parity_sample=%0b mode=0x%0h",
                  rtl_dbg_time(),
                  RTL_DBG_TIMEUNIT_STR,
                  data_q,
                  rx_sync_i,
                  active_parity_mode_q
                ));
              end
`endif

              s_current <= S_STOP;
            end else begin
              sample_count_q <= sample_count_q + 1'b1;
            end
          end
        end

        S_STOP: begin
          if (oversample_tick) begin
            if (int'(sample_count_q) == BIT_LAST_INT) begin
              sample_count_q <= '0;

              if (!rx_sync_i) begin
                frame_error_q   <= 1'b1;
                stop_low_seen_q <= 1'b1;
`ifndef SYNTHESIS
                if (RTL_DBG && !stop_low_seen_q) begin
                  `DPRINT($display(
                    "[RTL][WARN][%0.0f %s] %m: RX frame error, stop bit %0d sampled low",
                    rtl_dbg_time(),
                    RTL_DBG_TIMEUNIT_STR,
                    stop_bit_index_q
                  ));
                end
`endif
              end

              if (stop_bit_index_q == (active_stop_bits_q - 4'd1)) begin
                break_detect_q <= (frame_error_q || !rx_sync_i) &&
                                  all_data_zero_q &&
                                  (stop_low_seen_q || !rx_sync_i);
                stop_bit_index_q <= '0;
                s_current        <= S_DONE;
              end else begin
                stop_bit_index_q <= stop_bit_index_q + 4'd1;
              end
            end else begin
              sample_count_q <= sample_count_q + 1'b1;
            end
          end
        end

        S_DONE: begin
          rx_char_valid_q         <= 1'b1;
          event_rx_frame_error_q  <= frame_error_q;
          event_rx_parity_error_q <= parity_error_q;
          event_rx_break_detect_q <= break_detect_q;
          s_current               <= S_IDLE;
`ifndef SYNTHESIS
          if (RTL_DBG) begin
            if (frame_error_q || parity_error_q || break_detect_q) begin
              `DPRINT($display(
                "[RTL][WARN][%0.0f %s] %m: RX character completed with status, data=0x%02h frame=%0b parity=%0b break=%0b",
                rtl_dbg_time(),
                RTL_DBG_TIMEUNIT_STR,
                data_q,
                frame_error_q,
                parity_error_q,
                break_detect_q
              ));
            end else begin
              `DPRINT($display(
                "[RTL][INFO][%0.0f %s] %m: RX character completed, data=0x%02h",
                rtl_dbg_time(),
                RTL_DBG_TIMEUNIT_STR,
                data_q
              ));
            end
          end
`endif
        end

        default: begin
          s_current <= S_IDLE;
        end
      endcase
    end
  end

  assign rx_char_valid        = rx_char_valid_q;
  assign rx_char_data         = data_q;
  assign rx_char_frame_error  = frame_error_q;
  assign rx_char_parity_error = parity_error_q;
  assign rx_char_break_detect = break_detect_q;
  assign rx_busy              = (s_current != S_IDLE);
  assign event_rx_frame_error = event_rx_frame_error_q;
  assign event_rx_parity_error = event_rx_parity_error_q;
  assign event_rx_break_detect = event_rx_break_detect_q;

endmodule

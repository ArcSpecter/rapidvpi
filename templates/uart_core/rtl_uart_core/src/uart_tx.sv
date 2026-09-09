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

module uart_tx #(
  // ================================================================
  // DEBUG CONTROL
  // ================================================================
  parameter bit RTL_DBG = 1'b1
) (
  input  wire       clk,
  input  wire       rst_n,
  input  wire       cfg_enable,
  input  wire       cfg_tx_enable,
  input  wire [1:0] cfg_parity_mode,
  input  wire [1:0] cfg_stop_bits,
  input  wire [1:0] cfg_data_bits,
  input  wire       baud_tick,
  input  wire       tx_start,
  input  wire [7:0] tx_data,
  output wire       tx_ready_for_start,
  output wire       tx_busy,
  output wire       uart_tx_o,
  output wire       event_tx_done
);

  import uart_pkg::*;

  typedef enum logic [2:0] {
    S_IDLE,
    S_START,
    S_DATA,
    S_PARITY,
    S_STOP
  } uart_tx_state_t;

  uart_tx_state_t s_current;

  logic [7:0] data_q;
  logic [3:0] bit_index_q;
  logic [3:0] stop_bit_index_q;
  logic [3:0] active_data_bits_q;
  logic [3:0] active_stop_bits_q;
  logic [1:0] active_parity_mode_q;
  logic       parity_bit_q;
  logic       uart_tx_q;
  logic       event_tx_done_q;
  logic [2:0] next_bit_index;

  assign next_bit_index = bit_index_q[2:0] + 3'd1;

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

  function automatic logic data_parity(
    input logic [7:0] data,
    input logic [3:0] data_bits
  );
    begin
      case (data_bits)
        4'd5:    data_parity = ^data[4:0];
        4'd6:    data_parity = ^data[5:0];
        4'd7:    data_parity = ^data[6:0];
        default: data_parity = ^data[7:0];
      endcase
    end
  endfunction

  function automatic logic calc_parity_bit(
    input logic [7:0] data,
    input logic [3:0] data_bits,
    input logic [1:0] parity_mode
  );
    logic data_ones_odd;
    begin
      data_ones_odd = data_parity(data, data_bits);

      case (parity_mode)
        UART_PARITY_EVEN: calc_parity_bit = data_ones_odd;
        UART_PARITY_ODD:  calc_parity_bit = !data_ones_odd;
        default:          calc_parity_bit = 1'b0;
      endcase
    end
  endfunction

  always_ff @(posedge clk) begin
    if (!rst_n) begin
      s_current            <= S_IDLE;
      data_q               <= 8'h00;
      bit_index_q          <= '0;
      stop_bit_index_q     <= '0;
      active_data_bits_q   <= 4'd8;
      active_stop_bits_q   <= 4'd1;
      active_parity_mode_q <= UART_PARITY_NONE;
      parity_bit_q         <= 1'b0;
      uart_tx_q            <= 1'b1;
      event_tx_done_q      <= 1'b0;
    end else begin
      event_tx_done_q <= 1'b0;

      case (s_current)
        S_IDLE: begin
          uart_tx_q        <= 1'b1;
          bit_index_q      <= '0;
          stop_bit_index_q <= '0;

          if (tx_start && cfg_enable && cfg_tx_enable) begin
            s_current            <= S_START;
            data_q               <= tx_data;
            active_data_bits_q   <= decode_data_bits(cfg_data_bits);
            active_stop_bits_q   <= decode_stop_bits(cfg_stop_bits);
            active_parity_mode_q <= cfg_parity_mode;
            parity_bit_q         <= calc_parity_bit(tx_data,
                                                    decode_data_bits(cfg_data_bits),
                                                    cfg_parity_mode);
            uart_tx_q            <= 1'b0;
`ifndef SYNTHESIS
            if (RTL_DBG) begin
              `DPRINT($display(
                "[RTL][INFO][%0t] %m: TX character started, data=0x%02h data_bits=%0d stop_bits=%0d parity_mode=0x%0h parity_bit=%0b",
                $time,
                tx_data,
                decode_data_bits(cfg_data_bits),
                decode_stop_bits(cfg_stop_bits),
                cfg_parity_mode,
                calc_parity_bit(tx_data,
                                decode_data_bits(cfg_data_bits),
                                cfg_parity_mode)
              ));
            end
`endif
          end else if (tx_start) begin
`ifndef SYNTHESIS
            if (RTL_DBG) begin
              `DPRINT($display(
                "[RTL][WARN][%0t] %m: TX start ignored while disabled, cfg_enable=%0b cfg_tx_enable=%0b",
                $time,
                cfg_enable,
                cfg_tx_enable
              ));
            end
`endif
          end
        end

        S_START: begin
          if (baud_tick) begin
            s_current   <= S_DATA;
            bit_index_q <= '0;
            uart_tx_q   <= data_q[0];
          end
        end

        S_DATA: begin
          if (baud_tick) begin
            if (bit_index_q == (active_data_bits_q - 4'd1)) begin
              bit_index_q <= '0;

              if (parity_is_enabled(active_parity_mode_q)) begin
                s_current <= S_PARITY;
                uart_tx_q <= parity_bit_q;
              end else begin
                s_current        <= S_STOP;
                stop_bit_index_q <= '0;
                uart_tx_q        <= 1'b1;
              end
            end else begin
              bit_index_q <= bit_index_q + 4'd1;
              uart_tx_q   <= data_q[next_bit_index];
            end
          end
        end

        S_PARITY: begin
          if (baud_tick) begin
            s_current        <= S_STOP;
            stop_bit_index_q <= '0;
            uart_tx_q        <= 1'b1;
          end
        end

        S_STOP: begin
          uart_tx_q <= 1'b1;

          if (baud_tick) begin
            if (stop_bit_index_q == (active_stop_bits_q - 4'd1)) begin
              event_tx_done_q <= 1'b1;
              s_current       <= S_IDLE;
`ifndef SYNTHESIS
              if (RTL_DBG) begin
                `DPRINT($display(
                  "[RTL][INFO][%0t] %m: TX character completed, data=0x%02h stop_bits=%0d",
                  $time,
                  data_q,
                  active_stop_bits_q
                ));
              end
`endif
            end else begin
              stop_bit_index_q <= stop_bit_index_q + 4'd1;
            end
          end
        end

        default: begin
          s_current <= S_IDLE;
        end
      endcase
    end
  end

  assign tx_ready_for_start = (s_current == S_IDLE);
  assign tx_busy            = (s_current != S_IDLE);
  assign uart_tx_o          = uart_tx_q;
  assign event_tx_done      = event_tx_done_q;

endmodule

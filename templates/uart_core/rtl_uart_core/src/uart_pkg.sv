`default_nettype none

package uart_pkg;

  localparam logic [1:0] UART_PARITY_NONE = 2'd0;
  localparam logic [1:0] UART_PARITY_EVEN = 2'd1;
  localparam logic [1:0] UART_PARITY_ODD  = 2'd2;
  localparam logic [1:0] UART_PARITY_RSVD = 2'd3;

  localparam logic [1:0] UART_STOP_1     = 2'd0;
  localparam logic [1:0] UART_STOP_2     = 2'd1;
  localparam logic [1:0] UART_STOP_RSVD0 = 2'd2;
  localparam logic [1:0] UART_STOP_RSVD1 = 2'd3;

  localparam logic [1:0] UART_DATA_5 = 2'd0;
  localparam logic [1:0] UART_DATA_6 = 2'd1;
  localparam logic [1:0] UART_DATA_7 = 2'd2;
  localparam logic [1:0] UART_DATA_8 = 2'd3;

endpackage

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

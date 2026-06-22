# MIT License
#
# Copyright (c) 2026 Rovshan Rustamov
#
# Permission is hereby granted, free of charge, to any person obtaining a copy
# of this software and associated documentation files (the "Software"), to deal
# in the Software without restriction, including without limitation the rights
# to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
# copies of the Software, and to permit persons to whom the Software is
# furnished to do so, subject to the following conditions:
#
# The above copyright notice and this permission notice shall be included in all
# copies or substantial portions of the Software.
#
# THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
# IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
# FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
# AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
# LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
# OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE
# SOFTWARE.

onerror {resume}
quietly WaveActivateNextPane {} 0
add wave -noupdate -expand -group clock_reset -label clk /uart_core/clk
add wave -noupdate -expand -group clock_reset -label rst_n /uart_core/rst_n
add wave -noupdate -expand -group uart_pins -label uart_rx_i /uart_core/uart_rx_i
add wave -noupdate -expand -group uart_pins -label uart_tx_o /uart_core/uart_tx_o
add wave -noupdate -expand -group uart_pins -label uart_cts_i /uart_core/uart_cts_i
add wave -noupdate -expand -group uart_pins -label uart_rts_o /uart_core/uart_rts_o
add wave -noupdate -expand -group cfg -label cfg_enable /uart_core/cfg_enable
add wave -noupdate -expand -group cfg -label cfg_rx_enable /uart_core/cfg_rx_enable
add wave -noupdate -expand -group cfg -label cfg_tx_enable /uart_core/cfg_tx_enable
add wave -noupdate -expand -group cfg -label cfg_baud_inc /uart_core/cfg_baud_inc
add wave -noupdate -expand -group cfg -label cfg_parity_mode /uart_core/cfg_parity_mode
add wave -noupdate -expand -group cfg -label cfg_stop_bits /uart_core/cfg_stop_bits
add wave -noupdate -expand -group cfg -label cfg_data_bits /uart_core/cfg_data_bits
add wave -noupdate -expand -group cfg -label cfg_hw_flow_enable /uart_core/cfg_hw_flow_enable
add wave -noupdate -expand -group tx_stream -label tx_byte_valid /uart_core/tx_byte_valid
add wave -noupdate -expand -group tx_stream -label tx_byte_ready /uart_core/tx_byte_ready
add wave -noupdate -expand -group tx_stream -label tx_byte_data /uart_core/tx_byte_data
add wave -noupdate -expand -group rx_stream -label rx_byte_valid /uart_core/rx_byte_valid
add wave -noupdate -expand -group rx_stream -label rx_byte_ready /uart_core/rx_byte_ready
add wave -noupdate -expand -group rx_stream -label rx_byte_data /uart_core/rx_byte_data
add wave -noupdate -expand -group rx_stream -label rx_byte_frame_error /uart_core/rx_byte_frame_error
add wave -noupdate -expand -group rx_stream -label rx_byte_parity_error /uart_core/rx_byte_parity_error
add wave -noupdate -expand -group rx_stream -label rx_byte_break_detect /uart_core/rx_byte_break_detect
add wave -noupdate -expand -group fifo -label ctrl_rx_fifo_clear /uart_core/ctrl_rx_fifo_clear
add wave -noupdate -expand -group fifo -label ctrl_tx_fifo_clear /uart_core/ctrl_tx_fifo_clear
add wave -noupdate -expand -group fifo -label rx_fifo_level /uart_core/rx_fifo_level
add wave -noupdate -expand -group fifo -label tx_fifo_level /uart_core/tx_fifo_level
add wave -noupdate -expand -group fifo -label rx_fifo_empty /uart_core/rx_fifo_empty
add wave -noupdate -expand -group fifo -label rx_fifo_full /uart_core/rx_fifo_full
add wave -noupdate -expand -group fifo -label tx_fifo_empty /uart_core/tx_fifo_empty
add wave -noupdate -expand -group fifo -label tx_fifo_full /uart_core/tx_fifo_full
add wave -noupdate -expand -group status -label rx_busy /uart_core/rx_busy
add wave -noupdate -expand -group status -label tx_busy /uart_core/tx_busy
add wave -noupdate -expand -group status -label cts_active /uart_core/cts_active
add wave -noupdate -expand -group status -label rts_active /uart_core/rts_active
add wave -noupdate -expand -group status -label cts_blocked /uart_core/cts_blocked
add wave -noupdate -expand -group events -label event_rx_overrun /uart_core/event_rx_overrun
add wave -noupdate -expand -group events -label event_rx_frame_error /uart_core/event_rx_frame_error
add wave -noupdate -expand -group events -label event_rx_parity_error /uart_core/event_rx_parity_error
add wave -noupdate -expand -group events -label event_rx_break_detect /uart_core/event_rx_break_detect
add wave -noupdate -expand -group events -label event_tx_done /uart_core/event_tx_done
TreeUpdate [SetDefaultTree]
WaveRestoreCursors {{Cursor 1} {230 ns} 0}
quietly wave cursor active 1
configure wave -namecolwidth 320
configure wave -valuecolwidth 180
configure wave -justifyvalue left
configure wave -signalnamewidth 0
configure wave -snapdistance 10
configure wave -datasetprefix 0
configure wave -rowmargin 4
configure wave -childrowmargin 2
configure wave -gridoffset 0
configure wave -gridperiod 1
configure wave -griddelta 40
configure wave -timeline 0
configure wave -timelineunits ns
update
WaveRestoreZoom {0 ns} {7697 ns}

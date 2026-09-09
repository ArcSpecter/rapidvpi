# MIT License
#
# Copyright (c) 2024 Rovshan Rustamov
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

set rtl_uart_core_script_dir [file normalize [file dirname [info script]]]
set rtl_uart_core_src_dir    [file normalize [file join $rtl_uart_core_script_dir .. .. src]]

set rtl_uart_core_sv_files [list \
    [file join $rtl_uart_core_src_dir uart_pkg.sv] \
    [file join $rtl_uart_core_src_dir uart_rx_sync.sv] \
    [file join $rtl_uart_core_src_dir uart_baud_gen.sv] \
    [file join $rtl_uart_core_src_dir uart_byte_fifo.sv] \
    [file join $rtl_uart_core_src_dir uart_hw_flow_ctrl.sv] \
    [file join $rtl_uart_core_src_dir uart_rx.sv] \
    [file join $rtl_uart_core_src_dir uart_tx.sv] \
    [file join $rtl_uart_core_src_dir uart_core.sv] \
]

foreach f $rtl_uart_core_sv_files {
    if {![file exists $f]} {
        error "rtl_uart_core: missing source file: $f"
    }
}

read_verilog -sv $rtl_uart_core_sv_files

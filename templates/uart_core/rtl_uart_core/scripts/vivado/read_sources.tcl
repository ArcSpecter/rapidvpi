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

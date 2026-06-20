list(APPEND QUESTA_INIT_COMMANDS
    COMMAND ${CMAKE_COMMAND} -E chdir ${QUESTA_SIM_DIR}
    vlog -f ${VLOG_OPTIONS}
    -work ${WORK_LIB}
    ${CMAKE_CURRENT_LIST_DIR}/../../src/uart_pkg.sv

    COMMAND ${CMAKE_COMMAND} -E chdir ${QUESTA_SIM_DIR}
    vlog -f ${VLOG_OPTIONS}
    -work ${WORK_LIB}
    ${CMAKE_CURRENT_LIST_DIR}/../../src/uart_rx_sync.sv

    COMMAND ${CMAKE_COMMAND} -E chdir ${QUESTA_SIM_DIR}
    vlog -f ${VLOG_OPTIONS}
    -work ${WORK_LIB}
    ${CMAKE_CURRENT_LIST_DIR}/../../src/uart_baud_gen.sv

    COMMAND ${CMAKE_COMMAND} -E chdir ${QUESTA_SIM_DIR}
    vlog -f ${VLOG_OPTIONS}
    -work ${WORK_LIB}
    ${CMAKE_CURRENT_LIST_DIR}/../../src/uart_byte_fifo.sv

    COMMAND ${CMAKE_COMMAND} -E chdir ${QUESTA_SIM_DIR}
    vlog -f ${VLOG_OPTIONS}
    -work ${WORK_LIB}
    ${CMAKE_CURRENT_LIST_DIR}/../../src/uart_hw_flow_ctrl.sv

    COMMAND ${CMAKE_COMMAND} -E chdir ${QUESTA_SIM_DIR}
    vlog -f ${VLOG_OPTIONS}
    -work ${WORK_LIB}
    ${CMAKE_CURRENT_LIST_DIR}/../../src/uart_rx.sv

    COMMAND ${CMAKE_COMMAND} -E chdir ${QUESTA_SIM_DIR}
    vlog -f ${VLOG_OPTIONS}
    -work ${WORK_LIB}
    ${CMAKE_CURRENT_LIST_DIR}/../../src/uart_tx.sv

    COMMAND ${CMAKE_COMMAND} -E chdir ${QUESTA_SIM_DIR}
    vlog -f ${VLOG_OPTIONS}
    -work ${WORK_LIB}
    ${CMAKE_CURRENT_LIST_DIR}/../../src/uart_core.sv
)

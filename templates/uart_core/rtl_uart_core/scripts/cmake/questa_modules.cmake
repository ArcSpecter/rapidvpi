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

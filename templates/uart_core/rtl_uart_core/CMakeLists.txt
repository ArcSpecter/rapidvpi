cmake_minimum_required(VERSION 3.10)

project(rtl_uart_core VERSION 0.0.1 LANGUAGES NONE)

set(RTL_TOP_MODULE "dut_wrapper")
set(VIP_DESIGN_NAME "vip_uart_core")

set(VPI_MODULE_DIR "${CMAKE_SOURCE_DIR}/../${VIP_DESIGN_NAME}/cmake-build-release/")
set(VPI_MODULE_NAME "${VPI_MODULE_DIR}/lib${VIP_DESIGN_NAME}.so")

set(SRC_DIR "${CMAKE_SOURCE_DIR}/src")
set(EXT_DIR "${CMAKE_SOURCE_DIR}/ext")
set(SIM_WRAPPER_SOURCE "${CMAKE_SOURCE_DIR}/dut_wrapper.sv")

set(RTL_SOURCES
    "${SRC_DIR}/uart_pkg.sv"
    "${SRC_DIR}/uart_rx_sync.sv"
    "${SRC_DIR}/uart_baud_gen.sv"
    "${SRC_DIR}/uart_byte_fifo.sv"
    "${SRC_DIR}/uart_hw_flow_ctrl.sv"
    "${SRC_DIR}/uart_rx.sv"
    "${SRC_DIR}/uart_tx.sv"
    "${SRC_DIR}/uart_core.sv"
)

add_custom_target(ctags_rebuild
    COMMAND ctags
    -f ${CMAKE_SOURCE_DIR}/tags
    -R
    --fields=+nKz
    --languages=SystemVerilog
    --output-format=e-ctags
    --tag-relative=no
    src
    WORKING_DIRECTORY ${CMAKE_SOURCE_DIR}
    COMMENT "Rebuilding SystemVerilog ctags for ./src"
    VERBATIM
)

add_custom_target(lint
    COMMAND verilator
    -Wall --lint-only --sv --Wno-fatal --Wno-DECLFILENAME
    --top-module uart_core
    ${RTL_SOURCES}
    WORKING_DIRECTORY ${CMAKE_SOURCE_DIR}
    COMMENT "Running Verilator lint on uart_core RTL sources"
    VERBATIM
)

set(VLOG_OPTIONS
    -64
    -sv
)

set(VSIM_OPTIONS
    -64
    -t 1ps
    -onfinish stop
)


set(QUESTA_RUN_SCRIPT "${CMAKE_SOURCE_DIR}/scripts/sim/questa/questa_run.tcl")
set(QUESTA_RUN_GUI_SCRIPT "${CMAKE_SOURCE_DIR}/scripts/sim/questa/questa_run_gui.tcl")

set(QUESTA_SIM_DIR "${CMAKE_BINARY_DIR}/questa")
file(MAKE_DIRECTORY ${QUESTA_SIM_DIR})

include("${CMAKE_SOURCE_DIR}/scripts/cmake/rtl_dependency_helpers.cmake")

set(WORK_LIB "work")
set(WORK_LIB_DIR "${QUESTA_SIM_DIR}/${WORK_LIB}")

set(QUESTA_INIT_COMMANDS)
set(QUESTA_RTL_VLOG_COMMANDS)

foreach(RTL_SOURCE IN LISTS RTL_SOURCES)
    list(APPEND QUESTA_RTL_VLOG_COMMANDS
        COMMAND ${CMAKE_COMMAND} -E chdir ${QUESTA_SIM_DIR}
        vlog ${VLOG_OPTIONS}
        -work ${WORK_LIB}
        ${RTL_SOURCE}
    )
endforeach()

list(APPEND QUESTA_RTL_VLOG_COMMANDS
    COMMAND ${CMAKE_COMMAND} -E chdir ${QUESTA_SIM_DIR}
    vlog ${VLOG_OPTIONS}
    -work ${WORK_LIB}
    ${SIM_WRAPPER_SOURCE}
)

add_custom_target(
    sim_questa_init
    COMMAND ${CMAKE_COMMAND} -E remove_directory ${QUESTA_SIM_DIR}
    COMMAND ${CMAKE_COMMAND} -E make_directory ${QUESTA_SIM_DIR}
    COMMAND ${CMAKE_COMMAND} -E chdir ${QUESTA_SIM_DIR} vlib ${WORK_LIB}
    COMMAND vmap ${WORK_LIB} ${WORK_LIB_DIR}
    ${QUESTA_INIT_COMMANDS}
    ${QUESTA_RTL_VLOG_COMMANDS}
    COMMAND bash -c "cd ${QUESTA_SIM_DIR} && vmake ${WORK_LIB} > ${WORK_LIB}.mak"
    COMMENT "Initial QuestaSim compilation and vmake makefile generation"
    VERBATIM
)

add_custom_target(
    sim_questa_compile
    COMMAND ${CMAKE_COMMAND} -E remove -f ${QUESTA_SIM_DIR}/.stamp
    COMMAND ${CMAKE_COMMAND} -E echo "Rebuilding QuestaSim libraries via make ..."
    COMMAND ${CMAKE_COMMAND} -E chdir ${QUESTA_SIM_DIR} make -f ${WORK_LIB}.mak
    COMMAND ${CMAKE_COMMAND} -E echo "QuestaSim make finished"
    COMMAND ${CMAKE_COMMAND} -E touch ${QUESTA_SIM_DIR}/.stamp
    COMMENT "Compiling all units into libraries (incremental via vmake)"
)

add_custom_target(
    sim_questa_run
    COMMAND env LD_PRELOAD=/usr/lib64/libstdc++.so.6
    vsim -c
    ${VSIM_OPTIONS}
    ${RTL_TOP_MODULE}
    -pli ${VPI_MODULE_NAME}
    -do ${QUESTA_RUN_SCRIPT}
    WORKING_DIRECTORY ${QUESTA_SIM_DIR}
    DEPENDS sim_questa_compile
    COMMENT "Running QuestaSim simulation with modern libstdc++ (batch mode)"
)

add_custom_target(
    sim_questa_run_gui
    COMMAND env LD_PRELOAD=/usr/lib64/libstdc++.so.6
    vsim -gui
    ${VSIM_OPTIONS}
    ${RTL_TOP_MODULE}
    -pli ${VPI_MODULE_NAME}
    -do ${QUESTA_RUN_GUI_SCRIPT}
    WORKING_DIRECTORY ${QUESTA_SIM_DIR}
    DEPENDS sim_questa_compile
    COMMENT "Running QuestaSim simulation with modern libstdc++ (GUI mode)"
)

# =============================================================================
# Verilator simulation flow
# =============================================================================

set(VERILATOR_TOP_MODULE "dut_wrapper")

set(VERILATOR_SIM_DIR "${CMAKE_BINARY_DIR}/verilator")
set(VERILATOR_OBJ_DIR "${VERILATOR_SIM_DIR}/obj_dir")
set(VERILATOR_EXE_NAME "sim_verilator")
set(VERILATOR_EXE "${VERILATOR_OBJ_DIR}/${VERILATOR_EXE_NAME}")

set(VERILATOR_VPI_MODULE_DIR
    "${CMAKE_SOURCE_DIR}/../${VIP_DESIGN_NAME}/cmake-build-verilator"
)
set(VERILATOR_VPI_MODULE_NAME
    "${VERILATOR_VPI_MODULE_DIR}/lib${VIP_DESIGN_NAME}.so"
)

# This reusable IP has no external RTL dependencies. Verilator consumes the
# same dependency-ordered local product manifest followed by the wrapper.
set(VERILATOR_DEP_SOURCES)

set(VERILATOR_SIM_SOURCES
    ${VERILATOR_DEP_SOURCES}
    ${RTL_SOURCES}
    ${SIM_WRAPPER_SOURCE}
)

set(VERILATOR_OPTIONS
    -GHAS_RTS_CTS=0
    --binary
    --vpi
    --top-module ${VERILATOR_TOP_MODULE}
    --threads 1
    --public-flat-rw
    -Wno-fatal
    --timescale 1ns/1ps
    --default-language 1800-2012
    +1800-2012ext+sv
    --Mdir ${VERILATOR_OBJ_DIR}
    -o ${VERILATOR_EXE_NAME}
)

add_custom_target(
    sim_verilator_compile
    COMMAND ${CMAKE_COMMAND} -E make_directory ${VERILATOR_SIM_DIR}
    COMMAND verilator --version
    COMMAND verilator ${VERILATOR_OPTIONS} ${VERILATOR_SIM_SOURCES}
    WORKING_DIRECTORY ${CMAKE_SOURCE_DIR}
    COMMENT "Building dut_wrapper with Verilator VPI support (parameterized UART core)"
    USES_TERMINAL
    VERBATIM
)

add_custom_target(
    sim_verilator_run
    COMMAND ${CMAKE_COMMAND} -E echo
    "Using Verilator VPI plugin: ${VERILATOR_VPI_MODULE_NAME}"
    COMMAND bash -c
    "test -f '${VERILATOR_VPI_MODULE_NAME}' || { echo 'ERROR: Verilator VIP plugin missing: ${VERILATOR_VPI_MODULE_NAME}. Build ${VIP_DESIGN_NAME} target verilator_uart_core first.'; exit 1; }"
    COMMAND bash -c
    "test -x '${VERILATOR_EXE}' || { echo 'ERROR: Verilator executable missing: ${VERILATOR_EXE}'; exit 1; }"
    COMMAND ${VERILATOR_EXE}
    "+verilator+vpi+${VERILATOR_VPI_MODULE_NAME}"
    WORKING_DIRECTORY ${VERILATOR_SIM_DIR}
    DEPENDS sim_verilator_compile
    COMMENT "Running dut_wrapper with Verilator + RapidVPI (parameterized UART core)"
    USES_TERMINAL
    VERBATIM
)

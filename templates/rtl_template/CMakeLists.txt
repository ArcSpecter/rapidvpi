# MIT License

# Copyright (c) 2024 Rovshan Rustamov

# Permission is hereby granted, free of charge, to any person obtaining a copy
# of this software and associated documentation files (the "Software"), to deal
# in the Software without restriction, including without limitation the rights
# to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
# copies of the Software, and to permit persons to whom the Software is
# furnished to do so, subject to the following conditions:

# The above copyright notice and this permission notice shall be included in all
# copies or substantial portions of the Software.

# THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
# IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
# FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
# AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
# LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
# OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE
# SOFTWARE.

cmake_minimum_required(VERSION 3.10)

# Set the project and target name
set(PROJECT_NAME dut_top)
project(${PROJECT_NAME} VERSION 0.0.1)

# Define VPI directories for simulation
set(VPI_MODULE_DIR "${CMAKE_SOURCE_DIR}/../vip_template/cmake-build-debug/")
set(VPI_MODULE_NAME "${VPI_MODULE_DIR}/libvip_template.so")

# Define source directory and gather all Verilog & SystemVerilog source files
set(SRC_DIR "${CMAKE_SOURCE_DIR}/src")
file(GLOB_RECURSE SRC_FILES "${SRC_DIR}/*.sv") # ALL source files within ./src


#  ▄▄▄▄▄▄▄▄▄▄▄  ▄▄▄▄▄▄▄▄▄▄▄  ▄▄▄▄▄▄▄▄▄▄▄  ▄▄▄▄▄▄▄▄▄▄▄  ▄         ▄  ▄▄▄▄▄▄▄▄▄▄▄ 
# ▐░░░░░░░░░░░▌▐░░░░░░░░░░░▌▐░░░░░░░░░░░▌▐░░░░░░░░░░░▌▐░▌       ▐░▌▐░░░░░░░░░░░▌
#  ▀▀▀▀█░█▀▀▀▀ ▐░█▀▀▀▀▀▀▀▀▀ ▐░█▀▀▀▀▀▀▀█░▌▐░█▀▀▀▀▀▀▀█░▌▐░▌       ▐░▌▐░█▀▀▀▀▀▀▀▀▀ 
#      ▐░▌     ▐░▌          ▐░▌       ▐░▌▐░▌       ▐░▌▐░▌       ▐░▌▐░▌          
#      ▐░▌     ▐░▌          ▐░█▄▄▄▄▄▄▄█░▌▐░█▄▄▄▄▄▄▄█░▌▐░▌       ▐░▌▐░█▄▄▄▄▄▄▄▄▄ 
#      ▐░▌     ▐░▌          ▐░░░░░░░░░░░▌▐░░░░░░░░░░░▌▐░▌       ▐░▌▐░░░░░░░░░░░▌
#      ▐░▌     ▐░▌          ▐░█▀▀▀▀▀▀▀█░▌▐░█▀▀▀▀█░█▀▀ ▐░▌       ▐░▌ ▀▀▀▀▀▀▀▀▀█░▌
#      ▐░▌     ▐░▌          ▐░▌       ▐░▌▐░▌     ▐░▌  ▐░▌       ▐░▌          ▐░▌
#  ▄▄▄▄█░█▄▄▄▄ ▐░█▄▄▄▄▄▄▄▄▄ ▐░▌       ▐░▌▐░▌      ▐░▌ ▐░█▄▄▄▄▄▄▄█░▌ ▄▄▄▄▄▄▄▄▄█░▌
# ▐░░░░░░░░░░░▌▐░░░░░░░░░░░▌▐░▌       ▐░▌▐░▌       ▐░▌▐░░░░░░░░░░░▌▐░░░░░░░░░░░▌
#  ▀▀▀▀▀▀▀▀▀▀▀  ▀▀▀▀▀▀▀▀▀▀▀  ▀         ▀  ▀         ▀  ▀▀▀▀▀▀▀▀▀▀▀  ▀▀▀▀▀▀▀▀▀▀▀ 


# Define output for compilation
set(ICARUS_OUTPUT_DIR "${CMAKE_BINARY_DIR}/icarus")
set(OUTPUT_VVP "${ICARUS_OUTPUT_DIR}/${PROJECT_NAME}.vvp")

# Ensure the folder exists
file(MAKE_DIRECTORY ${ICARUS_OUTPUT_DIR})

# Define the command to compile the Verilog sources using iverilog
add_custom_command(
    OUTPUT ${OUTPUT_VVP}
    COMMAND ${CMAKE_COMMAND} -E make_directory ${ICARUS_OUTPUT_DIR}
    COMMAND iverilog
    -g 2012
    -o ${OUTPUT_VVP}
    -s ${PROJECT_NAME}
    -s dump
    ${SRC_FILES}
    ./dump.v
    DEPENDS ${SRC_FILES}
    WORKING_DIRECTORY ${CMAKE_SOURCE_DIR}
    COMMENT "Compiling Verilog sources to generate ${PROJECT_NAME}.vvp"
)

# Create a custom target to build the VVP file
add_custom_target(
    sim_icarus_compile ALL
    DEPENDS ${OUTPUT_VVP}
    COMMENT "Building Verilog simulation files in ${ICARUS_OUTPUT_DIR}"
)

# Create a custom target for running the simulation
add_custom_target(
    sim_icarus_run
    COMMAND vvp -M ${VPI_MODULE_DIR} -m ${VPI_MODULE_NAME} ${OUTPUT_VVP} -fst
    DEPENDS sim_icarus_compile
    COMMENT "Running simulation with vvp and VPI module"
)

# Create a custom target for launching GTKWave
add_custom_target(
    sim_wave
    COMMAND setsid gtkwave ${ICARUS_OUTPUT_DIR}/test.vcd > /dev/null 2>&1 &
    COMMENT "Launching GTKWave to view simulation results (detached)"
)

# Custom target for cleaning the ./sim subfolder
add_custom_target(
    clean_sim
    COMMAND ${CMAKE_COMMAND} -E rm -rf ${ICARUS_OUTPUT_DIR}
    COMMENT "Removing the ./sim subfolder (${ICARUS_OUTPUT_DIR})"
)



#  ▄▄▄▄▄▄▄▄▄▄▄  ▄         ▄  ▄▄▄▄▄▄▄▄▄▄▄  ▄▄▄▄▄▄▄▄▄▄▄  ▄▄▄▄▄▄▄▄▄▄▄  ▄▄▄▄▄▄▄▄▄▄▄ 
# ▐░░░░░░░░░░░▌▐░▌       ▐░▌▐░░░░░░░░░░░▌▐░░░░░░░░░░░▌▐░░░░░░░░░░░▌▐░░░░░░░░░░░▌
# ▐░█▀▀▀▀▀▀▀█░▌▐░▌       ▐░▌▐░█▀▀▀▀▀▀▀▀▀ ▐░█▀▀▀▀▀▀▀▀▀  ▀▀▀▀█░█▀▀▀▀ ▐░█▀▀▀▀▀▀▀█░▌
# ▐░▌       ▐░▌▐░▌       ▐░▌▐░▌          ▐░▌               ▐░▌     ▐░▌       ▐░▌
# ▐░▌       ▐░▌▐░▌       ▐░▌▐░█▄▄▄▄▄▄▄▄▄ ▐░█▄▄▄▄▄▄▄▄▄      ▐░▌     ▐░█▄▄▄▄▄▄▄█░▌
# ▐░▌       ▐░▌▐░▌       ▐░▌▐░░░░░░░░░░░▌▐░░░░░░░░░░░▌     ▐░▌     ▐░░░░░░░░░░░▌
# ▐░█▄▄▄▄▄▄▄█░▌▐░▌       ▐░▌▐░█▀▀▀▀▀▀▀▀▀  ▀▀▀▀▀▀▀▀▀█░▌     ▐░▌     ▐░█▀▀▀▀▀▀▀█░▌
# ▐░░░░░░░░░░░▌▐░▌       ▐░▌▐░▌                    ▐░▌     ▐░▌     ▐░▌       ▐░▌
#  ▀▀▀▀▀▀█░█▀▀ ▐░█▄▄▄▄▄▄▄█░▌▐░█▄▄▄▄▄▄▄▄▄  ▄▄▄▄▄▄▄▄▄█░▌     ▐░▌     ▐░▌       ▐░▌
#         ▐░▌  ▐░░░░░░░░░░░▌▐░░░░░░░░░░░▌▐░░░░░░░░░░░▌     ▐░▌     ▐░▌       ▐░▌
#          ▀    ▀▀▀▀▀▀▀▀▀▀▀  ▀▀▀▀▀▀▀▀▀▀▀  ▀▀▀▀▀▀▀▀▀▀▀       ▀       ▀         ▀ 


# QuestaSim tool options
set(VLOG_OPTIONS "${CMAKE_SOURCE_DIR}/scripts/sim/questa/vlog_options")
set(QUESTA_RUN_SCRIPT "${CMAKE_SOURCE_DIR}/scripts/sim/questa/questa_run.tcl")
set(QUESTA_RUN_GUI_SCRIPT "${CMAKE_SOURCE_DIR}/scripts/sim/questa/questa_run_gui.tcl")

set(QUESTA_SIM_DIR "${CMAKE_BINARY_DIR}/questa")
file(MAKE_DIRECTORY ${QUESTA_SIM_DIR})

# Logical libraries
set(WORK_LIB "work")
set(ADDER_LIB "adder")

# Physical locations for logical libraries
set(WORK_LIB_DIR "${QUESTA_SIM_DIR}/work")
set(ADDER_LIB_DIR "${QUESTA_SIM_DIR}/adder")


# 1) Initial auto-order compile + vmake generation
add_custom_target(
    sim_questa_init
    # Always start from a clean QuestaSim directory
    COMMAND ${CMAKE_COMMAND} -E remove_directory ${QUESTA_SIM_DIR}
    COMMAND ${CMAKE_COMMAND} -E make_directory ${QUESTA_SIM_DIR}

    # Create libraries
    COMMAND ${CMAKE_COMMAND} -E chdir ${QUESTA_SIM_DIR} vlib ${WORK_LIB}
    COMMAND ${CMAKE_COMMAND} -E chdir ${QUESTA_SIM_DIR} vlib ${ADDER_LIB}

    # Map libraries
    COMMAND vmap ${WORK_LIB} ${WORK_LIB_DIR}
    COMMAND vmap ${ADDER_LIB} ${ADDER_LIB_DIR}

    # RTL and libraries compilation
    COMMAND ${CMAKE_COMMAND} -E chdir ${QUESTA_SIM_DIR} vlog -f ${VLOG_OPTIONS} -work ${WORK_LIB} ${SRC_DIR}/dut_top.sv
    COMMAND ${CMAKE_COMMAND} -E chdir ${QUESTA_SIM_DIR} vlog -f ${VLOG_OPTIONS} -work ${ADDER_LIB} ${SRC_DIR}/clocked_adder.sv

    # Generate makefile for incremental builds (needs shell for '>')
    COMMAND bash -c "cd ${QUESTA_SIM_DIR} && vmake ${WORK_LIB} > ${WORK_LIB}.mak"
    COMMAND bash -c "cd ${QUESTA_SIM_DIR} && vmake ${ADDER_LIB} > ${ADDER_LIB}.mak"

    COMMENT "Initial QuestaSim compilation and vmake makefile generation (fresh)"
    VERBATIM
)

# 2) Incremental compile via vmake + make
add_custom_target(
    sim_questa_compile ALL
    COMMAND ${CMAKE_COMMAND} -E remove -f ${QUESTA_SIM_DIR}/.stamp
    COMMAND ${CMAKE_COMMAND} -E echo "Rebuilding QuestaSim work library via make ..."
    COMMAND ${CMAKE_COMMAND} -E chdir ${QUESTA_SIM_DIR} make -f ${WORK_LIB}.mak
    COMMAND ${CMAKE_COMMAND} -E chdir ${QUESTA_SIM_DIR} make -f ${ADDER_LIB}.mak
    COMMAND ${CMAKE_COMMAND} -E echo "QuestaSim make finished"
    COMMAND ${CMAKE_COMMAND} -E touch ${QUESTA_SIM_DIR}/.stamp
    COMMENT "Compiling all units into libraries (incremental via vmake)"
)

# 3) Batch (console) run
add_custom_target(
    sim_questa_run
    COMMAND env LD_PRELOAD=/usr/lib64/libstdc++.so.6
    vsim -c
    -L ${WORK_LIB}
    -L ${ADDER_LIB}
    ${PROJECT_NAME}
    -pli ${VPI_MODULE_NAME}
    -do ${QUESTA_RUN_SCRIPT}
    WORKING_DIRECTORY ${QUESTA_SIM_DIR}
    DEPENDS sim_questa_compile
    COMMENT "Running QuestaSim simulation with modern libstdc++ (batch mode)"
)

# 4) GUI run
add_custom_target(
    sim_questa_run_gui
    COMMAND env LD_PRELOAD=/usr/lib64/libstdc++.so.6
    vsim -gui
    -L ${WORK_LIB}
    -L ${ADDER_LIB}
    ${PROJECT_NAME}
    -pli ${VPI_MODULE_NAME}
    -do ${QUESTA_RUN_GUI_SCRIPT}
    WORKING_DIRECTORY ${QUESTA_SIM_DIR}
    DEPENDS sim_questa_compile
    COMMENT "Running QuestaSim simulation with modern libstdc++ (GUI mode)"
)

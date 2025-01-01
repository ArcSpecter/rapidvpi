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
file(GLOB_RECURSE SRC_FILES "${SRC_DIR}/*.v" "${SRC_DIR}/*.sv")

# Define output for compilation in the ./sim subfolder
set(SIM_OUTPUT_DIR "${CMAKE_BINARY_DIR}/sim")
set(OUTPUT_VVP "${SIM_OUTPUT_DIR}/${PROJECT_NAME}.vvp")

# Ensure the ./sim subfolder exists
file(MAKE_DIRECTORY ${SIM_OUTPUT_DIR})

# Define the command to compile the Verilog sources using iverilog
add_custom_command(
    OUTPUT ${OUTPUT_VVP}
    COMMAND ${CMAKE_COMMAND} -E make_directory ${SIM_OUTPUT_DIR}
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
    sim_compile ALL
    DEPENDS ${OUTPUT_VVP}
    COMMENT "Building Verilog simulation files in ./sim"
)

# Create a custom target for running the simulation
add_custom_target(
    sim_run
    COMMAND vvp -M ${VPI_MODULE_DIR} -m ${VPI_MODULE_NAME} ${OUTPUT_VVP}
    DEPENDS sim_compile
    COMMENT "Running simulation with vvp and VPI module"
)

# Create a custom target for launching GTKWave
add_custom_target(
    sim_wave
    COMMAND setsid gtkwave ${SIM_OUTPUT_DIR}/test.vcd > /dev/null 2>&1 &
    COMMENT "Launching GTKWave to view simulation results (detached)"
)

# Custom target for cleaning the ./sim subfolder
add_custom_target(
    clean_sim
    COMMAND ${CMAKE_COMMAND} -E rm -rf ${SIM_OUTPUT_DIR}
    COMMENT "Removing the ./sim subfolder (${SIM_OUTPUT_DIR})"
)

# Custom target for cleaning the entire build directory
add_custom_target(
    clean_all
    COMMAND ${CMAKE_COMMAND} -E rm -rf ${CMAKE_BINARY_DIR}
    COMMENT "Removing the entire build directory (${CMAKE_BINARY_DIR})"
)
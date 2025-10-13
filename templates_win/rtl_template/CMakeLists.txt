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

# Project
set(PROJECT_NAME dut_top)
project(${PROJECT_NAME} VERSION 0.0.1)

# ---- Paths ----
# Where the user VIP (built as a VPI) lives
set(VIP_BUILD_DIR "${CMAKE_SOURCE_DIR}/../vip_template/cmake-build-release")
set(VPI_MODULE_PATH "${VIP_BUILD_DIR}/vip_template.vpi") # produced by vip_template (see note above)

# HDL sources
set(SRC_DIR "${CMAKE_SOURCE_DIR}/src")
file(GLOB_RECURSE SRC_FILES "${SRC_DIR}/*.v" "${SRC_DIR}/*.sv")

# Output dir for compiled sim
set(SIM_OUTPUT_DIR "${CMAKE_BINARY_DIR}/sim")
set(OUTPUT_VVP "${SIM_OUTPUT_DIR}/${PROJECT_NAME}.vvp")

# Ensure ./sim exists
file(MAKE_DIRECTORY "${SIM_OUTPUT_DIR}")

# ---- Compile HDL -> .vvp ----
add_custom_command(
  OUTPUT "${OUTPUT_VVP}"
  COMMAND ${CMAKE_COMMAND} -E make_directory "${SIM_OUTPUT_DIR}"
  COMMAND iverilog
  -g 2012
  -o "${OUTPUT_VVP}"
  -s ${PROJECT_NAME}
  -s dump
  ${SRC_FILES}
  "${CMAKE_SOURCE_DIR}/dump.v"
  DEPENDS ${SRC_FILES} "${CMAKE_SOURCE_DIR}/dump.v"
  WORKING_DIRECTORY "${CMAKE_SOURCE_DIR}"
  COMMENT "Compiling Verilog/SystemVerilog to ${OUTPUT_VVP}"
)

add_custom_target(sim_compile ALL
  DEPENDS "${OUTPUT_VVP}"
  COMMENT "Building Verilog simulation files in ./sim"
)

# ---- Run simulation ----
# vvp notes:
# -M adds a module search path (we point it at the VIP build dir)
# -m can accept a full path on MSYS2; we pass the .vpi path explicitly.
add_custom_target(sim_run
  COMMAND vvp -M "${VIP_BUILD_DIR}" -m "${VPI_MODULE_PATH}" "${OUTPUT_VVP}"
  DEPENDS sim_compile
  COMMENT "Running simulation with vvp and vip_template.vpi"
)

# ---- Optional: launch GTKWave (detached) ----
add_custom_target(sim_wave
  COMMAND setsid gtkwave "${SIM_OUTPUT_DIR}/test.vcd" > /dev/null 2>&1 &
  COMMENT "Launching GTKWave (detached)"
)

# ---- Clean helpers ----
add_custom_target(clean_sim
  COMMAND ${CMAKE_COMMAND} -E rm -rf "${SIM_OUTPUT_DIR}"
  COMMENT "Removing ${SIM_OUTPUT_DIR}"
)

add_custom_target(clean_all
  COMMAND ${CMAKE_COMMAND} -E rm -rf "${CMAKE_BINARY_DIR}"
  COMMENT "Removing the entire build directory (${CMAKE_BINARY_DIR})"
)







# --- Questa: directory and exe
# Tip: leave forward slashes; CMake handles them on Windows.
set(QUESTA_DIR "F:/eda/quartus_24_1_lite/questa_fse/win64" CACHE PATH "Path to Questa 'win64' bin dir")
set(VSIM_EXE   "${QUESTA_DIR}/vsim.exe")

add_custom_target(sim_questa
  COMMAND "${VSIM_EXE}"
  COMMENT "Launching vsim"
  USES_TERMINAL
)

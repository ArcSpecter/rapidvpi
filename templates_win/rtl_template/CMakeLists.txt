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

# ============================================================
# VPI / VIP paths (Windows)
# ============================================================
# Where the user VIP (built as a VPI) lives
# (vip_template builds vip_template.vpi in cmake-build-release)
set(VIP_BUILD_DIR "${CMAKE_SOURCE_DIR}/../vip_template/cmake-build-release")
set(VPI_MODULE_PATH "${VIP_BUILD_DIR}/vip_template.vpi") # full .vpi path

# ============================================================
# HDL sources (common)
# ============================================================
set(SRC_DIR "${CMAKE_SOURCE_DIR}/src")
file(GLOB_RECURSE SRC_FILES
  "${SRC_DIR}/*.sv"
  "${SRC_DIR}/*.v"
)

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

# ============================================================
# Icarus flow (iverilog + vvp + vip_template.vpi)
# ============================================================
set(ICARUS_OUTPUT_DIR "${CMAKE_BINARY_DIR}/icarus")
set(OUTPUT_VVP "${ICARUS_OUTPUT_DIR}/${PROJECT_NAME}.vvp")

# Ensure the folder exists
file(MAKE_DIRECTORY ${ICARUS_OUTPUT_DIR})

# Compile Verilog/SV to .vvp
add_custom_command(
  OUTPUT ${OUTPUT_VVP}
  COMMAND ${CMAKE_COMMAND} -E make_directory ${ICARUS_OUTPUT_DIR}
  COMMAND iverilog
  -g 2012
  -o ${OUTPUT_VVP}
  -s ${PROJECT_NAME}
  -s dump
  ${SRC_FILES}
  "${CMAKE_SOURCE_DIR}/dump.v"
  DEPENDS ${SRC_FILES} "${CMAKE_SOURCE_DIR}/dump.v"
  WORKING_DIRECTORY ${CMAKE_SOURCE_DIR}
  COMMENT "Compiling Verilog sources to generate ${PROJECT_NAME}.vvp"
)

# Build vvp
add_custom_target(
  sim_icarus_compile ALL
  DEPENDS ${OUTPUT_VVP}
  COMMENT "Building Verilog simulation files in ${ICARUS_OUTPUT_DIR}"
)

# Run vvp with VPI module
# vvp: -M <dir> + -m <full .vpi path> works on MSYS2
add_custom_target(
  sim_icarus_run
  COMMAND vvp -M "${VIP_BUILD_DIR}" -m "${VPI_MODULE_PATH}" "${OUTPUT_VVP}" -fst
  DEPENDS sim_icarus_compile
  COMMENT "Running simulation with vvp and vip_template.vpi"
)

# Launch GTKWave (simple blocking call; no setsid on Windows)
add_custom_target(
  sim_wave
  COMMAND gtkwave "${ICARUS_OUTPUT_DIR}/test.vcd"
  COMMENT "Launching GTKWave to view simulation results"
)

# Clean Icarus output
add_custom_target(
  clean_sim
  COMMAND ${CMAKE_COMMAND} -E rm -rf ${ICARUS_OUTPUT_DIR}
  COMMENT "Removing ${ICARUS_OUTPUT_DIR}"
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

# ============================================================
# QuestaSim (Windows) – init / compile / run / GUI
# ============================================================
# ============================================================
# QuestaSim (Windows) – init / compile / run / GUI
# ============================================================

# Tool options + run scripts (mirroring Linux layout)
set(VLOG_OPTIONS "${CMAKE_SOURCE_DIR}/scripts/sim/questa/vlog_options")
set(QUESTA_RUN_SCRIPT "${CMAKE_SOURCE_DIR}/scripts/sim/questa/questa_run.tcl")
set(QUESTA_RUN_GUI_SCRIPT "${CMAKE_SOURCE_DIR}/scripts/sim/questa/questa_run_gui.tcl")

# --- Questa: bin dir and executables (Windows)
# Tip: use forward slashes; CMake is fine with that.
set(QUESTA_DIR "C:/temp_backup/eda/quartus_24_1_lite/questa_fse/win64"
  CACHE PATH "Path to Questa 'win64' bin dir")

set(VLIB_EXE "${QUESTA_DIR}/vlib.exe")
set(VMAP_EXE "${QUESTA_DIR}/vmap.exe")
set(VLOG_EXE "${QUESTA_DIR}/vlog.exe")
set(VSIM_EXE "${QUESTA_DIR}/vsim.exe")

# Questa working directory under the build tree
set(QUESTA_SIM_DIR "${CMAKE_BINARY_DIR}/questa")
file(MAKE_DIRECTORY ${QUESTA_SIM_DIR})

# Logical libraries
set(WORK_LIB "work")
set(ADDER_LIB "adder")

# Physical locations for logical libraries
set(WORK_LIB_DIR "${QUESTA_SIM_DIR}/work")
set(ADDER_LIB_DIR "${QUESTA_SIM_DIR}/adder")

# Single stamp that represents "libs created + mapped + compiled"
set(QUESTA_COMPILE_STAMP "${QUESTA_SIM_DIR}/.questa_compile.stamp")

# This command does what your Linux sim_questa_init did:
#   - clean dir
#   - vlib work/adder
#   - vmap
#   - vlog dut_top + clocked_adder
add_custom_command(
  OUTPUT ${QUESTA_COMPILE_STAMP}
  COMMAND ${CMAKE_COMMAND} -E echo "QuestaSim full compile (Windows: clean + libs + vlog)..."
  # Clean / recreate sim dir
  COMMAND ${CMAKE_COMMAND} -E remove_directory ${QUESTA_SIM_DIR}
  COMMAND ${CMAKE_COMMAND} -E make_directory ${QUESTA_SIM_DIR}

  # Create libraries
  COMMAND ${CMAKE_COMMAND} -E chdir ${QUESTA_SIM_DIR} "${VLIB_EXE}" ${WORK_LIB}
  COMMAND ${CMAKE_COMMAND} -E chdir ${QUESTA_SIM_DIR} "${VLIB_EXE}" ${ADDER_LIB}

  # Map libraries
  COMMAND ${CMAKE_COMMAND} -E chdir ${QUESTA_SIM_DIR} "${VMAP_EXE}" ${WORK_LIB} ${WORK_LIB_DIR}
  COMMAND ${CMAKE_COMMAND} -E chdir ${QUESTA_SIM_DIR} "${VMAP_EXE}" ${ADDER_LIB} ${ADDER_LIB_DIR}

  # RTL and libraries compilation
  COMMAND ${CMAKE_COMMAND} -E chdir ${QUESTA_SIM_DIR} "${VLOG_EXE}" -f ${VLOG_OPTIONS} -work ${WORK_LIB} ${SRC_DIR}/dut_top.sv
  COMMAND ${CMAKE_COMMAND} -E chdir ${QUESTA_SIM_DIR} "${VLOG_EXE}" -f ${VLOG_OPTIONS} -work ${ADDER_LIB} ${SRC_DIR}/clocked_adder.sv

  # Stamp to tell Ninja/CMake it's up to date
  COMMAND ${CMAKE_COMMAND} -E touch ${QUESTA_COMPILE_STAMP}

  DEPENDS
  ${SRC_DIR}/dut_top.sv
  ${SRC_DIR}/clocked_adder.sv
  ${VLOG_OPTIONS}

  COMMENT "QuestaSim: fresh libs + vlog (Windows)"
  VERBATIM
)

# 1) Initial init target – just depends on the same stamp
#    (so first run does full build; later runs are no-op if nothing changed)
add_custom_target(
  sim_questa_init
  DEPENDS ${QUESTA_COMPILE_STAMP}
)

# 2) "Compile" target – same as init, but kept for symmetry / scripts
add_custom_target(
  sim_questa_compile
  DEPENDS ${QUESTA_COMPILE_STAMP}
)

# 3) Batch (console) run
add_custom_target(
  sim_questa_run
  COMMAND "${VSIM_EXE}" -c
  -L ${WORK_LIB}
  -L ${ADDER_LIB}
  ${PROJECT_NAME}
  -pli "${VPI_MODULE_PATH}"
  -do ${QUESTA_RUN_SCRIPT}
  WORKING_DIRECTORY ${QUESTA_SIM_DIR}
  DEPENDS sim_questa_compile
  COMMENT "Running QuestaSim simulation (Windows, batch mode)"
  VERBATIM
)

# 4) GUI run
add_custom_target(
  sim_questa_run_gui
  COMMAND "${VSIM_EXE}" -gui
  -L ${WORK_LIB}
  -L ${ADDER_LIB}
  ${PROJECT_NAME}
  -pli "${VPI_MODULE_PATH}"
  -do ${QUESTA_RUN_GUI_SCRIPT}
  WORKING_DIRECTORY ${QUESTA_SIM_DIR}
  DEPENDS sim_questa_compile
  COMMENT "Running QuestaSim simulation (Windows, GUI mode)"
  VERBATIM
)

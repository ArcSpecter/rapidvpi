cmake_minimum_required(VERSION 3.10)

set(PROJECT_NAME vip_uart_core)
project(${PROJECT_NAME} VERSION 0.0.1)

set(CMAKE_CXX_STANDARD 23)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

set(VIP_VPI_FLAVOR "EXISTING" CACHE STRING
        "VPI build flavor: EXISTING or VERILATOR")
set_property(CACHE VIP_VPI_FLAVOR PROPERTY STRINGS EXISTING VERILATOR)

if (NOT VIP_VPI_FLAVOR STREQUAL "EXISTING" AND
        NOT VIP_VPI_FLAVOR STREQUAL "VERILATOR")
    message(FATAL_ERROR
            "Unsupported VIP_VPI_FLAVOR='${VIP_VPI_FLAVOR}'; expected EXISTING or VERILATOR.")
endif ()

if (VIP_VPI_FLAVOR STREQUAL "VERILATOR")
    find_program(VERILATOR_EXECUTABLE NAMES verilator REQUIRED)

    execute_process(
            COMMAND "${VERILATOR_EXECUTABLE}" --getenv VERILATOR_ROOT
            RESULT_VARIABLE _verilator_getenv_result
            OUTPUT_VARIABLE VERILATOR_ROOT
            OUTPUT_STRIP_TRAILING_WHITESPACE
            ERROR_QUIET
    )

    if (NOT _verilator_getenv_result EQUAL 0 OR
            NOT EXISTS "${VERILATOR_ROOT}/include/vltstd/vpi_user.h")
        execute_process(
                COMMAND "${VERILATOR_EXECUTABLE}" -V
                RESULT_VARIABLE _verilator_version_result
                OUTPUT_VARIABLE _verilator_version_output
                ERROR_VARIABLE _verilator_version_error
        )
        set(_verilator_version_text
                "${_verilator_version_output}\n${_verilator_version_error}")
        string(REGEX MATCH
                "VERILATOR_ROOT[ \t]*=[ \t]*([^\r\n]+)"
                _verilator_root_match
                "${_verilator_version_text}")
        set(VERILATOR_ROOT "${CMAKE_MATCH_1}")
        string(STRIP "${VERILATOR_ROOT}" VERILATOR_ROOT)
    endif ()

    if (NOT EXISTS "${VERILATOR_ROOT}/include/vltstd/vpi_user.h")
        message(FATAL_ERROR
                "Could not find Verilator's standard VPI header under VERILATOR_ROOT='${VERILATOR_ROOT}'.")
    endif ()

    set(vpi_include_dir "${VERILATOR_ROOT}/include/vltstd" CACHE PATH
            "Path to the VPI include directory" FORCE)
    message(STATUS "Verilator VPI header: ${vpi_include_dir}/vpi_user.h")
else ()
    set(vpi_include_dir "$ENV{VPI_INCLUDE_DIR}" CACHE PATH
            "Path to the VPI include directory")

    if (NOT vpi_include_dir)
        message(FATAL_ERROR
                "VPI_INCLUDE_DIR environment variable must be set or provided manually.")
    endif ()
endif ()

set(CMAKE_PREFIX_PATH "/usr/local/lib/cmake/rapidvpi")
find_package(rapidvpi REQUIRED)

set(TEST_SOURCES
        src/test.cpp
        src/init.cpp
)

add_library(${PROJECT_NAME} SHARED ${TEST_SOURCES})

target_include_directories(${PROJECT_NAME} PRIVATE
        /usr/local/include/rapidvpi/core
        /usr/local/include/rapidvpi/scheduler
        /usr/local/include/rapidvpi/testbase
        /usr/local/include/rapidvpi/testmanager
        src
        ext
        ${vpi_include_dir}
)

add_subdirectory(ext/vip_common)
add_subdirectory(ext/vip_uart)
add_subdirectory(src/scoreboard)
add_subdirectory(src/agents)
add_subdirectory(src/cases)

find_library(ZSTD_LIB zstd REQUIRED)

target_link_libraries(${PROJECT_NAME} PRIVATE rapidvpi::rapidvpi.vpi)
#target_link_libraries(${PROJECT_NAME} PRIVATE vpi) # Only needed for Iverilog
if (VIP_VPI_FLAVOR STREQUAL "EXISTING")
    target_link_libraries(${PROJECT_NAME} PRIVATE vpi)
endif ()
target_link_libraries(${PROJECT_NAME} PRIVATE ${ZSTD_LIB})

if (NOT VIP_VPI_FLAVOR STREQUAL "VERILATOR")
    set(VERILATOR_BUILD_DIR
            "${CMAKE_SOURCE_DIR}/cmake-build-verilator")

    add_custom_target(verilator_uart_core
            COMMAND "${CMAKE_COMMAND}"
                    -S "${CMAKE_SOURCE_DIR}"
                    -B "${VERILATOR_BUILD_DIR}"
                    -G "${CMAKE_GENERATOR}"
                    -DCMAKE_BUILD_TYPE:STRING=Release
                    -DCMAKE_C_COMPILER:FILEPATH=${CMAKE_C_COMPILER}
                    -DCMAKE_CXX_COMPILER:FILEPATH=${CMAKE_CXX_COMPILER}
                    -DVIP_VPI_FLAVOR:STRING=VERILATOR
            COMMAND "${CMAKE_COMMAND}"
                    --build "${VERILATOR_BUILD_DIR}"
                    --target ${PROJECT_NAME}
                    --parallel
            BYPRODUCTS
                    "${VERILATOR_BUILD_DIR}/lib${PROJECT_NAME}.so"
            USES_TERMINAL
            VERBATIM
    )
endif ()

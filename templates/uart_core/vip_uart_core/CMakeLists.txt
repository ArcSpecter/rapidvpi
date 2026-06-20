cmake_minimum_required(VERSION 3.50)

set(PROJECT_NAME vip_uart_core)
project(${PROJECT_NAME} VERSION 0.0.1)

set(CMAKE_CXX_STANDARD 23)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

set(vpi_include_dir $ENV{VPI_INCLUDE_DIR} CACHE PATH "Path to the VPI include directory")
if (NOT vpi_include_dir)
    message(FATAL_ERROR "VPI_INCLUDE_DIR environment variable must be set or provided manually.")
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
target_link_libraries(${PROJECT_NAME} PRIVATE ${ZSTD_LIB})

set(TEST_SUBFOLDER uart_core_scoreboard)

add_library(${TEST_SUBFOLDER} OBJECT
        scb_uart_core.cpp
)

target_include_directories(${TEST_SUBFOLDER} PRIVATE
        ${CMAKE_SOURCE_DIR}/src
        ${CMAKE_SOURCE_DIR}/ext
        /usr/local/include/rapidvpi/core
        /usr/local/include/rapidvpi/scheduler
        /usr/local/include/rapidvpi/testbase
        /usr/local/include/rapidvpi/testmanager
        ${vpi_include_dir}
)

set_property(TARGET ${TEST_SUBFOLDER} PROPERTY POSITION_INDEPENDENT_CODE ON)

target_sources(${PROJECT_NAME} PRIVATE $<TARGET_OBJECTS:${TEST_SUBFOLDER}>)

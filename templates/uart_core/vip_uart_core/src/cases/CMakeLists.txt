set(TEST_SUBFOLDER cases)

add_library(${TEST_SUBFOLDER} OBJECT
        tc_utils.cpp
        tc_basic.cpp
        tc_cfg.cpp
        tc_fifo.cpp
        tc_error.cpp
        tc_flow_ctrl.cpp
        tc_reset.cpp
        tc_phase.cpp
        tc_stress_no_cts.cpp
)

target_include_directories(${TEST_SUBFOLDER} PRIVATE
        ${CMAKE_CURRENT_SOURCE_DIR}
        ${CMAKE_CURRENT_SOURCE_DIR}/..
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

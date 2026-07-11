# Unit tests for smart glasses charger firmware
# Runs on host PC with mocked hardware layer

cmake_minimum_required(VERSION 3.13)
project(sgc_tests C)

set(CMAKE_C_STANDARD 11)
set(CMAKE_C_FLAGS "-Wall -Wextra -g -O0 --coverage")

enable_testing()

# Mock SPL layer - stubs out hardware register access
add_library(sgc_mock STATIC
    ${CMAKE_SOURCE_DIR}/mock/mock_spl.c
    ${CMAKE_SOURCE_DIR}/mock/mock_stubs.c
    ${CMAKE_SOURCE_DIR}/../src/hal/hal_gpio.c
    ${CMAKE_SOURCE_DIR}/../src/app/charge_flow.c
)
target_include_directories(sgc_mock PUBLIC
    ${CMAKE_SOURCE_DIR}/mock
    ${CMAKE_SOURCE_DIR}/../src
    ${CMAKE_SOURCE_DIR}/../src/hal
    ${CMAKE_SOURCE_DIR}/../src/protocol
    ${CMAKE_SOURCE_DIR}/../src/app
    ${CMAKE_SOURCE_DIR}/../src/drivers
)

# Test framework: we use a minimal assert-based approach (no external deps)
# Each test file is a separate executable

# Helper macro to add a test
function(add_sgc_test name src_file)
    add_executable(test_${name} ${src_file})
    target_link_libraries(test_${name} sgc_mock)
    target_include_directories(test_${name} PRIVATE
        ${CMAKE_SOURCE_DIR}/../src
        ${CMAKE_SOURCE_DIR}/../src/hal
        ${CMAKE_SOURCE_DIR}/../src/protocol
        ${CMAKE_SOURCE_DIR}/../src/app
        ${CMAKE_SOURCE_DIR}/../src/drivers
        ${CMAKE_SOURCE_DIR}/common
    )
    add_test(NAME ${name} COMMAND test_${name})
endfunction()

# GPIO HAL tests
add_sgc_test(gpio_config test_gpio_config.c)

# Protocol tests (no hardware dependency)
add_sgc_test(protocol test_protocol.c)
target_sources(test_protocol PRIVATE
    ${CMAKE_SOURCE_DIR}/../src/protocol/at_crc.c
    ${CMAKE_SOURCE_DIR}/../src/protocol/at_frame.c
)

# Charge flow + aux logic tests
add_sgc_test(charge_flow test_charge_flow.c)
target_sources(test_charge_flow PRIVATE
    ${CMAKE_SOURCE_DIR}/../src/app/aux_logic.c
)

# State machine transition tests (provides own mocks, no sgc_mock link)
add_sgc_test(state_machine test_state_machine.c)
target_sources(test_state_machine PRIVATE
    ${CMAKE_SOURCE_DIR}/../src/app/state_machine.c
    ${CMAKE_SOURCE_DIR}/../src/app/aux_logic.c
)
target_link_libraries(test_state_machine PRIVATE)

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
    ${CMAKE_SOURCE_DIR}/../src/hal/hal_gpio.c
)
target_include_directories(sgc_mock PUBLIC
    ${CMAKE_SOURCE_DIR}/mock
    ${CMAKE_SOURCE_DIR}/../src
    ${CMAKE_SOURCE_DIR}/../src/hal
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
        ${CMAKE_SOURCE_DIR}/common
    )
    add_test(NAME ${name} COMMAND test_${name})
endfunction()

# GPIO HAL tests
add_sgc_test(gpio_config test_gpio_config.c)

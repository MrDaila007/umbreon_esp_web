cmake_minimum_required(VERSION 3.16)
project(umbreon_tests C)

# Host-side unit tests — compiled with GCC/Clang, NOT xtensa cross-compiler
set(CMAKE_C_STANDARD 99)
set(CMAKE_C_FLAGS "${CMAKE_C_FLAGS} -Wall -Wextra -Werror -g")

# ws_handshake.c is compiled with UNIT_TEST to expose static functions
# and strip socket/mbedTLS dependencies
add_definitions(-DUNIT_TEST)

# Include paths
include_directories(
    ${CMAKE_CURRENT_SOURCE_DIR}                         # for unity/, mocks/
    ${CMAKE_CURRENT_SOURCE_DIR}/../include              # app_config.h, led_logic.h
    ${CMAKE_CURRENT_SOURCE_DIR}/mocks                   # mock_freertos.h, mock_esp_log.h
)

# Unity test framework
set(UNITY_SRC unity/unity.c)

# Source under test (ws_handshake.c with UNIT_TEST guard)
set(SRC_UNDER_TEST
    ${CMAKE_CURRENT_SOURCE_DIR}/../main/ws_handshake.c
)

# Test sources
set(TEST_SOURCES
    test_base64.c
    test_header_parser.c
    test_led_logic.c
    test_ws_frame.c
    run_all_tests.c
)

add_executable(run_tests
    ${UNITY_SRC}
    ${SRC_UNDER_TEST}
    ${TEST_SOURCES}
)

# ws_handshake.h lives in main/ (alongside the .c file)
target_include_directories(run_tests PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}/../main
)

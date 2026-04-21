cmake_minimum_required(VERSION 3.16)
project(ungula_lib_sd_tests)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

set(FETCHCONTENT_BASE_DIR ${CMAKE_CURRENT_LIST_DIR}/vendor)

include(FetchContent)

FetchContent_Declare(
    googletest
    GIT_REPOSITORY https://github.com/google/googletest.git
    GIT_TAG v1.14.0
)
FetchContent_MakeAvailable(googletest)

set(LIB_SD_SRC ${CMAKE_CURRENT_LIST_DIR}/../src)

include_directories(
    ${LIB_SD_SRC}
    ${CMAKE_CURRENT_LIST_DIR}/ArduinoStub
)

# Host build — interfaces only, no ESP32 platform code.
add_compile_definitions(
)

# No library .cpp files needed — the interfaces are header-only and
# the ESP32 implementation is gated behind #ifdef ESP_PLATFORM.
add_executable(test_sd
    test_sd.cpp
)
target_link_libraries(test_sd gtest_main)

enable_testing()
include(GoogleTest)
gtest_discover_tests(test_sd)

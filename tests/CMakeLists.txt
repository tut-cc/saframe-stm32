# Host-side tests for the modules that do not depend on the HAL or uT-Kernel.
# Built as a separate CMake project because the root CMakeLists.txt configures
# the arm-none-eabi cross toolchain.
#
#   cd tests && cmake --preset host-test \
#     && cmake --build --preset host-test && ctest --preset host-test

cmake_minimum_required(VERSION 3.22)

project(saframe_host_tests C)

enable_testing()

add_executable(privacy_filter_test
    ${CMAKE_CURRENT_SOURCE_DIR}/privacy_filter_test.c
    ${CMAKE_CURRENT_SOURCE_DIR}/../Appli/FaceDetection/Src/privacy_filter.c
)

set_target_properties(privacy_filter_test PROPERTIES
    C_STANDARD 11
    C_STANDARD_REQUIRED ON
    C_EXTENSIONS OFF
)

target_include_directories(privacy_filter_test PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}/../Appli/FaceDetection/Inc
)

if(NOT CMAKE_C_COMPILER_ID MATCHES "GNU|Clang")
    message(FATAL_ERROR "Host tests need GCC or Clang (sanitizers).")
endif()

target_compile_options(privacy_filter_test PRIVATE
    -Wall -Wextra -Werror -fsanitize=address,undefined -fno-omit-frame-pointer
)
target_link_options(privacy_filter_test PRIVATE
    -fsanitize=address,undefined
)

add_test(NAME privacy_filter COMMAND privacy_filter_test)

# LeakSanitizer is unavailable on macOS; on Linux it is on by default. The test
# has no allocations, so disable it there to keep both platforms identical.
if(APPLE)
    set_tests_properties(privacy_filter PROPERTIES
        ENVIRONMENT "ASAN_OPTIONS=detect_leaks=0")
endif()

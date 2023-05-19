cmake_minimum_required(VERSION 3.0.0)
project(wt VERSION 0.1.0)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_FLAGS_DEBUG "${CMAKE_CXX_FLAGS} -fsanitize=address -fsanitize=undefined -g3 -O0")

find_package(GTest REQUIRED)

file(GLOB WT_SRC *.cpp *.h)
file(GLOB WT_TEST_SRC tests/*.cpp tests/*.h)

include_directories(.)
add_executable(wt_test
    ${WT_SRC}
    ${WT_TEST_SRC}
)

target_link_libraries(
  wt_test
  GTest::gtest_main
)

include(GoogleTest)

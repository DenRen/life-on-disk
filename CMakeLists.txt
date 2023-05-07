cmake_minimum_required(VERSION 3.0.0)
project(main VERSION 0.1.0)

# include(CTest)
# enable_testing()

# set(CMAKE_CXX_FLAGS_DEBUG "${CMAKE_CXX_FLAGS} -fsanitize=address -fsanitize=undefined -g3 -O0")

file(GLOB sources main.cpp src/*.cpp src/*.h)
add_executable(main ${sources})

set(CPACK_PROJECT_NAME ${PROJECT_NAME})
set(CPACK_PROJECT_VERSION ${PROJECT_VERSION})

# DNA Tests
enable_testing()

find_package(GTest REQUIRED)

file(GLOB SRC src/*.cpp src/*.h)
file(GLOB DNA_SRC dna/*.cpp dna/*.h)
file(GLOB DNA_TEST_SRC dna/tests/*.cpp)

add_executable(
    dna_test
    ${SRC}
    ${DNA_SRC}
    ${DNA_TEST_SRC}
)
target_link_libraries(
  dna_test
  GTest::gtest_main
)

include(GoogleTest)
gtest_discover_tests(dna_test)
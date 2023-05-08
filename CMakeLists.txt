cmake_minimum_required(VERSION 3.0.0)
project(main VERSION 0.1.0)

# include(CTest)
# enable_testing()

# set(CMAKE_CXX_FLAGS_DEBUG "${CMAKE_CXX_FLAGS} -fsanitize=address -fsanitize=undefined -g3 -O0")

file(GLOB COMMON_SRC common/*.cpp common/*.h)

# Text main
file(GLOB TEXT_SRC text/*.cpp text/*.h)

add_executable(text_main
    text_main.cpp
    ${TEXT_SRC}
    ${COMMON_SRC}
)

# Dna main
file(GLOB DNA_SRC dna/*.cpp dna/*.h)

# add_executable(dna_main
#     text_main.cpp
#     ${DNA_SRC}
#     ${COMMON_SRC}
# )

# DNA Tests
enable_testing()

find_package(GTest REQUIRED)

file(GLOB DNA_TEST_SRC dna/tests/*.cpp)

add_executable(
    dna_test
    ${DNA_SRC}
    ${COMMON_SRC}
    ${DNA_TEST_SRC}
)
target_link_libraries(
  dna_test
  GTest::gtest_main
)

include(GoogleTest)
gtest_discover_tests(dna_test)

# Utilites

add_executable(
    convert_text_dna
    utilits/convert_text_dna.cpp
    ${SRC}
    ${DNA_SRC}
    ${COMMON_SRC}
)

add_executable(
    comp_to_suff_arr
    utilits/comp_to_suff_arr.cpp
    ${SRC}
    ${DNA_SRC}
    ${COMMON_SRC}
)

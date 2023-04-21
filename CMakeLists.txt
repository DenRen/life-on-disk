cmake_minimum_required(VERSION 3.0.0)
project(main VERSION 0.1.0)

# include(CTest)
# enable_testing()

file(GLOB sources main.cpp src/*.cpp src/*.h)
add_executable(main ${sources})

set(CPACK_PROJECT_NAME ${PROJECT_NAME})
set(CPACK_PROJECT_VERSION ${PROJECT_VERSION})

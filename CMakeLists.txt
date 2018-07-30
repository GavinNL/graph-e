project(FrameGraph)

cmake_minimum_required(VERSION 2.8.12)

add_definitions("-std=c++17")

#include(${CMAKE_BINARY_DIR}/conanbuildinfo.cmake)
#conan_basic_setup()

add_executable(main main.cpp)
target_link_libraries(main ${CONAN_LIBS} pthread)

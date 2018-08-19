project(graph_schedule)

cmake_minimum_required(VERSION 2.8.12)

add_definitions("-std=c++17")

       add_executable(example_1_serial
                      example_1_serial.cpp)
target_link_libraries(example_1_serial)

       add_executable(example_2_threadpool
                      example_2_threadpool.cpp)
target_link_libraries(example_2_threadpool pthread)


       add_executable(example_3_oneshot
                      example_3_oneshot.cpp)
target_link_libraries(example_3_oneshot pthread)

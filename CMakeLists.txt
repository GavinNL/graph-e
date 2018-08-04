project(FrameGraph)

cmake_minimum_required(VERSION 2.8.12)

add_definitions("-std=c++17")

add_executable(main main.cpp)
target_link_libraries(main pthread)

       add_executable(ext_thread_pool
                      ext_thread_pool.cpp)
target_link_libraries(ext_thread_pool pthread)

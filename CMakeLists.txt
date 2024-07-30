cmake_minimum_required(VERSION 3.28)
project(videoplayer)

add_subdirectory(deps/mp4v2)

set(CMAKE_CXX_STANDARD 20)

add_executable(videoplayer main.cpp)

target_link_libraries(videoplayer PRIVATE mp4v2)

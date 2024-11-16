cmake_minimum_required(VERSION 3.28)
project(videoplayer CXX ASM)
list(APPEND CMAKE_MODULE_PATH ${CMAKE_CURRENT_SOURCE_DIR}/cmake)

set(CMAKE_CXX_STANDARD 23)
find_package(bento4 REQUIRED)
find_package(glm REQUIRED)

add_subdirectory(shaders)

add_executable(videoplayer main.cpp
        MP4.h
        MP4.cpp
        H264.cpp
        H264.h
        Gfx.cpp
        Gfx.h
)

target_link_libraries(videoplayer PRIVATE bento4::ap4 glm shaders)
target_include_directories(videoplayer PRIVATE deps/include)
target_compile_options(videoplayer PRIVATE -Wall -Wpedantic -Wextra)
wut_create_rpx(videoplayer)
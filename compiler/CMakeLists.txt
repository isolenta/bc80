cmake_minimum_required(VERSION 3.31)
project(bc80_compiler C)

include_directories(${CMAKE_SOURCE_DIR})
set(CMAKE_C_FLAGS "${CMAKE_C_FLAGS} -g -ggdb -O0 -fno-omit-frame-pointer -std=c11")
set(BISON_EXECUTABLE /opt/homebrew/opt/bison/bin/bison)
set(FLEX_EXECUTABLE /opt/homebrew/opt/flex/bin/flex)
set(CMAKE_C_COMPILER gcc)

add_subdirectory(common)
add_subdirectory(asm)
add_subdirectory(rccompiler)

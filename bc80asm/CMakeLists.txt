cmake_minimum_required(VERSION 3.31)
project(bc80asm C)

include_directories(${CMAKE_SOURCE_DIR})
set(CMAKE_C_FLAGS "${CMAKE_C_FLAGS} -g -ggdb -O0 -fno-omit-frame-pointer -std=c11")
set(BISON_EXECUTABLE /opt/homebrew/opt/bison/bin/bison)
set(FLEX_EXECUTABLE /opt/homebrew/opt/flex/bin/flex)
set(CMAKE_C_COMPILER gcc)

add_subdirectory(bits)
add_subdirectory(asm)
add_subdirectory(disasm)

add_custom_target(test
  COMMAND ${CMAKE_CURRENT_SOURCE_DIR}/test.sh ${CMAKE_CURRENT_SOURCE_DIR}/bc80asm ${CMAKE_CURRENT_SOURCE_DIR}/bc80dasm $ENV{TEST}
  WORKING_DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR})
add_dependencies(test bc80asm_install bc80dasm_install)

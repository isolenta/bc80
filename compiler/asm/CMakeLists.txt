# cmake_minimum_required(VERSION 3.31)
# project(assembler C)

set(ASM_C_SOURCES
  compile.c
  disas_opcodes.c
  disas_print.c
  instruction.c
  libasm.c
  parse.c
  render.c
  render_elf.c
  render_sna.c
)

SET(PARSER_SOURCE ${CMAKE_CURRENT_BINARY_DIR}/parser.tab.c)
SET(LEXER_SOURCE ${CMAKE_CURRENT_BINARY_DIR}/lexer.yy.c)
SET(LEXER_HEADER ${CMAKE_CURRENT_BINARY_DIR}/lexer.yy.h)

add_custom_command(
  OUTPUT ${PARSER_SOURCE}
  COMMAND ${BISON_EXECUTABLE} -v -d -o ${PARSER_SOURCE} ${CMAKE_CURRENT_SOURCE_DIR}/parser.y
  DEPENDS ${CMAKE_CURRENT_SOURCE_DIR}/parser.y
  COMMENT "Generating parser"
)

add_custom_command(
  OUTPUT ${LEXER_SOURCE}
  COMMAND ${FLEX_EXECUTABLE} --header-file=${LEXER_HEADER} -o ${LEXER_SOURCE} ${CMAKE_CURRENT_SOURCE_DIR}/lexer.l
  DEPENDS ${CMAKE_CURRENT_SOURCE_DIR}/lexer.l
  COMMENT "Generating lexer"
)

add_custom_target(generate_sources ALL DEPENDS ${PARSER_SOURCE} ${LEXER_SOURCE})

add_library(asm STATIC ${ASM_C_SOURCES} ${PARSER_SOURCE} ${LEXER_SOURCE})
add_dependencies(asm generate_sources)
target_include_directories(asm PUBLIC ${CMAKE_CURRENT_BINARY_DIR})

add_executable(as as.c)
target_link_libraries(as PRIVATE asm common)

add_executable(disas disas.c)
target_link_libraries(disas PRIVATE asm common)

add_custom_target(test
  COMMAND ${CMAKE_CURRENT_SOURCE_DIR}/test.sh ${CMAKE_CURRENT_BINARY_DIR}/as ${CMAKE_CURRENT_BINARY_DIR}/disas $ENV{TEST}
  WORKING_DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR})
add_dependencies(test as)
add_dependencies(test disas)

install(TARGETS as DESTINATION ${CMAKE_SOURCE_DIR})
install(TARGETS disas DESTINATION ${CMAKE_SOURCE_DIR})

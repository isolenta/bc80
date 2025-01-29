# cmake_minimum_required(VERSION 3.31)
# project(rccompiler C)

set(RCC_C_SOURCES
  codegen.c
  debug_dump.c
  parse.c
  preproc.c
  rcc.c
  semantics.c
  semantics_global.c
)

SET(PARSER_SOURCE ${CMAKE_CURRENT_BINARY_DIR}/rc_parser.tab.c)
SET(LEXER_SOURCE ${CMAKE_CURRENT_BINARY_DIR}/rc_lexer.yy.c)
SET(LEXER_HEADER ${CMAKE_CURRENT_BINARY_DIR}/rc_lexer.yy.h)

add_custom_command(
  OUTPUT ${PARSER_SOURCE}
  COMMAND ${BISON_EXECUTABLE} -v -d -o ${PARSER_SOURCE} ${CMAKE_CURRENT_SOURCE_DIR}/rc_parser.y
)

add_custom_command(
  OUTPUT ${LEXER_SOURCE}
  COMMAND ${FLEX_EXECUTABLE} --header-file=${LEXER_HEADER} -o ${LEXER_SOURCE} ${CMAKE_CURRENT_SOURCE_DIR}/rc_lexer.l
)

add_custom_target(gen_rcc_parser ALL DEPENDS ${PARSER_SOURCE})
add_custom_target(gen_rcc_lexer ALL DEPENDS ${LEXER_SOURCE})

add_executable(rcc ${RCC_C_SOURCES})
add_dependencies(rcc gen_rcc_parser)
add_dependencies(rcc gen_rcc_lexer)
target_sources(rcc PRIVATE ${PARSER_SOURCE})
target_sources(rcc PRIVATE ${LEXER_SOURCE})
target_include_directories(rcc PUBLIC ${CMAKE_CURRENT_BINARY_DIR})
target_link_libraries(rcc PRIVATE asm common)

install(TARGETS rcc DESTINATION ${CMAKE_SOURCE_DIR})

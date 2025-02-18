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

SET(RC_PARSER_SOURCE ${CMAKE_CURRENT_BINARY_DIR}/rc_parser.tab.c)
SET(RC_LEXER_SOURCE ${CMAKE_CURRENT_BINARY_DIR}/rc_lexer.yy.c)
SET(RC_LEXER_HEADER ${CMAKE_CURRENT_BINARY_DIR}/rc_lexer.yy.h)

add_custom_command(
  OUTPUT ${RC_PARSER_SOURCE}
  COMMAND ${BISON_EXECUTABLE} -v -d -o ${RC_PARSER_SOURCE} ${CMAKE_CURRENT_SOURCE_DIR}/rc_parser.y
  DEPENDS ${CMAKE_CURRENT_SOURCE_DIR}/rc_parser.y
  COMMENT "Generating parser"
)

add_custom_command(
  OUTPUT ${RC_LEXER_SOURCE}
  COMMAND ${FLEX_EXECUTABLE} --header-file=${RC_LEXER_HEADER} -o ${RC_LEXER_SOURCE} ${CMAKE_CURRENT_SOURCE_DIR}/rc_lexer.l
  DEPENDS ${CMAKE_CURRENT_SOURCE_DIR}/rc_lexer.l
  COMMENT "Generating lexer"
)

add_custom_target(rcc_generate_sources ALL DEPENDS ${RC_PARSER_SOURCE} ${RC_LEXER_SOURCE})

add_executable(rcc ${RCC_C_SOURCES} ${RC_PARSER_SOURCE} ${RC_LEXER_SOURCE})
add_dependencies(rcc rcc_generate_sources)
target_include_directories(rcc PUBLIC ${CMAKE_CURRENT_BINARY_DIR})
target_link_libraries(rcc PRIVATE asm common)

install(TARGETS rcc DESTINATION ${CMAKE_SOURCE_DIR})

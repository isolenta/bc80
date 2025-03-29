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

add_executable(bc80asm
  bc80asm.c
  compile.c
  expressions.c
  symtab.c
  instruction.c
  parse.c
  parse_dump.c
  render.c
  render_elf.c
  render_sna.c
  ${PARSER_SOURCE}
  ${LEXER_SOURCE}
)
target_link_libraries(bc80asm PRIVATE bits)
target_include_directories(bc80asm PUBLIC ${CMAKE_CURRENT_BINARY_DIR})
add_dependencies(bc80asm generate_sources)

install(TARGETS bc80asm DESTINATION ${CMAKE_SOURCE_DIR})

add_custom_target(bc80asm_install
    DEPENDS bc80asm
    COMMAND ${CMAKE_COMMAND} --build . --target install
    COMMENT "installing bc80asm"
)

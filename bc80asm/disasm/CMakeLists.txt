add_executable(bc80dasm
  disasm.c
  disasm_opcodes.c
  disasm_print.c
)
target_link_libraries(bc80dasm PRIVATE bits)

install(TARGETS bc80dasm DESTINATION ${CMAKE_SOURCE_DIR})

add_custom_target(bc80dasm_install
    DEPENDS bc80dasm
    COMMAND ${CMAKE_COMMAND} --build . --target install
    COMMENT "installing bc80dasm"
)

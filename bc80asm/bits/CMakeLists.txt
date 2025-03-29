set(BITS_C_SOURCES
  buffer.c
  common.c
  dynarray.c
  error.c
  filesystem.c
  hashmap.c
  mmgr.c
)
add_library(bits STATIC ${BITS_C_SOURCES})

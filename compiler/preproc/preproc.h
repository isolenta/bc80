#pragma once

#include <setjmp.h>
#include "error.h"
#include "hashmap.h"

typedef struct preproc_ctx_t {
  hashmap *constants;
  hashmap *macroses;
  dynarray *includeopts;
  uint32_t unique_counter;
  error_callback_type error_cb;
  warning_callback_type warning_cb;
  jmp_buf *error_jmp_env;
  hashmap *processed_files;
} preproc_ctx_t;

extern char *read_file_content(preproc_ctx_t *ctx, char *filename);
extern char *scan(preproc_ctx_t *ctx, const char *source, const char *filename);

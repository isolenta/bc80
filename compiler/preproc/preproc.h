#pragma once

#include <setjmp.h>
#include "error.h"
#include "hashmap.h"

typedef struct preproc_ctx_t {
  hashmap *constants;
  dynarray *includeopts;
  error_callback_type error_cb;
  warning_callback_type warning_cb;
  jmp_buf *error_jmp_env;
} preproc_ctx_t;

extern char *read_file_content(preproc_ctx_t *ctx, char *filename);
extern char *do_preproc(preproc_ctx_t *ctx, const char *source, const char *filename);

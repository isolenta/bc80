#pragma once

#include "asm/libasm.h"
#include "common/error.h"

typedef struct hashmap hashmap;
typedef struct dynarray dynarray;

typedef struct rcc_ctx_t {
  hashmap *constants;
  dynarray *includeopts;
  char *in_filename;
} rcc_ctx_t;

#define ERROR_VERBOSE (1 << 0)

#define report_error(ctx, flags, fmt, ...) \
  do { \
    if ((ctx)->error_cb) \
      generic_report_error((ctx)->error_cb, (ctx)->error_jmp_env, (ctx)->in_filename, (flags & ERROR_VERBOSE) ? (ctx)->node->line : 0, fmt, ## __VA_ARGS__); \
  } while(0)


#define report_warning(ctx, flags, fmt, ...) \
  do { \
    if ((ctx)->warning_cb) \
      generic_report_warning((ctx)->warning_cb, (ctx)->in_filename, (flags & ERROR_VERBOSE) ? (ctx)->node->line : 0, fmt, ## __VA_ARGS__); \
  } while (0)

extern char *do_preproc(rcc_ctx_t *ctx, const char *source);
extern void *do_parse(rcc_ctx_t *ctx, char *source);
extern char *dump_ast(void *ast);
extern void ast_free(void *ast);
extern char *do_compile(rcc_ctx_t *ctx, void *ast);

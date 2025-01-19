#pragma once

#include "asm/libasm.h"
#include "common/error.h"
#include "rcc/parser.h"

typedef struct hashmap hashmap;
typedef struct dynarray dynarray;
typedef struct ParseNode ParseNode;

typedef struct rcc_ctx_t {
  hashmap *constants;
  dynarray *includeopts;
  char *in_filename;
  hashmap *pp_files;
  dynarray *pp_cond_stack;
  ParseNode *parse_tree_top;
  char *pp_output_str;
  struct scanner_state scanner_state;
  struct parser_state parser_state;
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

typedef void* yyscan_t;

extern char *do_preproc(rcc_ctx_t *ctx, const char *source);

extern void *do_parse(rcc_ctx_t *ctx, char *source);
extern char *dump_parse_tree(void *parse_tree);
extern void parse_tree_free(void *parse_tree);

extern void *do_ast(rcc_ctx_t *ctx, void *parse_tree);
extern char *dump_ast(void *ast);
extern void ast_free(void *ast);

extern char *do_compile(rcc_ctx_t *ctx, void *ast);

extern bool is_keyword(const char *id);
extern bool is_operator(const char *id);
extern bool is_identifier(const char *id);
extern char *get_current_token(yyscan_t scanner);
extern int get_actual_position(rcc_ctx_t *ctx, int scanner_pos, char **filename);
#pragma once

#include "asm/libasm.h"
#include "common/error.h"
#include "rccompiler/parser.h"

typedef struct hashmap hashmap;
typedef struct dynarray dynarray;
typedef struct Node Node;

typedef struct position_t {
  char *filename;
  int line;
  int pos;
} position_t;

typedef struct rcc_ctx_t {
  hashmap *constants;
  dynarray *includeopts;
  char *in_filename;
  hashmap *pp_files;
  dynarray *pp_cond_stack;
  Node *parse_tree_top;
  char *pp_output_str;
  struct scanner_state scanner_state;
  struct parser_state parser_state;
  position_t current_position;

  hashmap *global_symtab;
  hashmap *func_decls;
  hashmap *type_decls;
} rcc_ctx_t;

#define report_error(ctx, fmt, ...) \
  generic_report_error( (ERROR_OUT_LOC | ERROR_OUT_LINE | ERROR_OUT_POS), \
                        (ctx)->current_position.filename, \
                        (ctx)->current_position.line, \
                        (ctx)->current_position.pos, \
                        fmt, ## __VA_ARGS__);

#define report_warning(ctx, fmt, ...) \
  generic_report_warning( (ERROR_OUT_LOC | ERROR_OUT_LINE | ERROR_OUT_POS), \
                        (ctx)->current_position.filename, \
                        (ctx)->current_position.line, \
                        (ctx)->current_position.pos, \
                        fmt, ## __VA_ARGS__);

typedef void* yyscan_t;

extern char *do_preproc(rcc_ctx_t *ctx, const char *source);

extern void *do_parse(rcc_ctx_t *ctx, char *source);
extern char *dump_parse_tree(void *parse_tree);
extern void parse_tree_free(void *parse_tree);

extern void do_semantics(rcc_ctx_t *ctx, void *parse_tree);
extern char *dump_semantics(rcc_ctx_t *ctx);
extern void semantics_free(rcc_ctx_t *ctx);

extern char *do_codegen(rcc_ctx_t *ctx);

extern bool is_keyword(const char *id);
extern bool is_operator(const char *id);
extern bool is_identifier(const char *id);
extern char *get_current_token(yyscan_t scanner);
extern int get_actual_position(rcc_ctx_t *ctx, int scanner_pos, char **filename);
extern char *get_token_at(char *source, int line, int pos, int token_len);

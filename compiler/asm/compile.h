#pragma once

#include <setjmp.h>
#include <stdint.h>

#include "common/error.h"
#include "asm/parse.h"
#include "asm/libasm.h"

typedef struct buffer buffer;
typedef struct dynarray dynarray;
typedef struct hashmap hashmap;

typedef struct {
  uint32_t start;
  uint32_t curr_pc;
  buffer *content;
  char *name;
  uint8_t filler;
} section_ctx_t;

typedef struct {
  int start_iter;
  int count;
  int counter;
  char *varname;
  int rept_node_line;
} rept_ctx_t;

typedef struct {
  int cycles;
  int bytes;
} profile_data_t;

typedef struct {
  EXPR *expr;
  bool cond_value;
  bool inverse;
  int if_node_line;
} condition_ctx_t;

typedef struct compile_ctx_t {
  parse_node *node;
  dynarray *patches;
  hashmap *symtab;
  bool verbose_error;
  dynarray *sections;
  int curr_section_id;
  int target;
  bool sna_generic;
  int sna_pc_addr;
  int sna_ramtop;
  char *curr_global_label;
  error_callback_type error_cb;
  warning_callback_type warning_cb;
  jmp_buf *error_jmp_env;

  // stacked contexts for rept..endr statements
  dynarray *repts;
  char *lookup_rept_suffix;

  // stacked contexts for if..else..endif statements
  dynarray *conditions;

  // expr_eval() will set it to true if any compile-time arithmetics were performed
  bool was_literal_evals;

  bool in_profile;
  profile_data_t current_profile;
  char *current_profile_name;
} compile_ctx_t;

typedef struct {
  parse_node *node;
  uint32_t pos;
  int nbytes;
  bool relative;
  uint32_t instr_pc;
  int section_id;
  char *rept_suffix;
} patch_t;

#define report_error(ctx, fmt, ...) \
  do { \
    generic_report_error((ctx)->verbose_error ? (ERROR_OUT_LOC | ERROR_OUT_LINE | ERROR_OUT_POS) : 0, \
      (ctx)->node->fn, (ctx)->node->line - 1, (ctx)->node->pos, \
      fmt, ## __VA_ARGS__); \
  } while (0)


#define report_warning(ctx, fmt, ...) \
  do { \
    generic_report_warning((ctx)->verbose_error ? (ERROR_OUT_LOC | ERROR_OUT_LINE) : 0, \
      (ctx)->node->fn, (ctx)->node->line, 0, \
      fmt, ## __VA_ARGS__); \
  } while (0)

#define report_info(ctx, fmt, ...) \
  do { \
    generic_report_info(0, \
      (ctx)->node->fn, (ctx)->node->line, 0, \
      fmt, ## __VA_ARGS__); \
  } while (0)

extern parse_node *expr_eval(compile_ctx_t *ctx, parse_node *node);
extern int compile(struct libasm_as_desc_t *desc, dynarray *parse);
extern void compile_instruction_impl(compile_ctx_t *ctx, char *name, dynarray *args);
extern void register_fwd_lookup(compile_ctx_t *ctx,
                          parse_node *unresolved_node,
                          uint32_t pos,
                          int nbytes,
                          bool relative,
                          uint32_t instr_pc);

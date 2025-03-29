#pragma once

#include <setjmp.h>
#include <stdint.h>

#include "asm/parse.h"
#include "asm/bc80asm.h"
#include "bits/error.h"

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
  compile_opts opts;

  parse_node *node;
  dynarray *patches;
  hashmap *symtab;
  dynarray *sections;
  int curr_section_id;
  char *curr_global_label;

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

extern parse_node *expr_eval(compile_ctx_t *ctx, parse_node *node);
extern uint32_t compile(compile_opts opts, hashmap *defineopts, dynarray *statements, char **dest_buf);
extern void compile_instruction_impl(compile_ctx_t *ctx, char *name, dynarray *instr_args);
extern void register_fwd_lookup(compile_ctx_t *ctx,
                          parse_node *unresolved_node,
                          uint32_t pos,
                          int nbytes,
                          bool relative,
                          uint32_t instr_pc);

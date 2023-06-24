#pragma once

#include "common.h"
#include "buffer.h"
#include "libasm80.h"

typedef struct {
  uint32_t start;
  uint32_t curr_pc;
  buffer *content;
  char *name;
  uint8_t filler;
} section_ctx_t;

typedef struct {
  parse_node *node;
  dynarray *patches;
  hashmap *symtab;
  bool verbose_error;
  dynarray *sections;
  int curr_section_id;
  int target;
  error_callback_type error_cb;
  warning_callback_type warning_cb;
  jmp_buf *error_jmp_env;
} compile_ctx_t;

typedef struct {
  parse_node *node;
  uint32_t pos;
  int nbytes;
  bool relative;
  uint32_t instr_pc;
  int section_id;
} patch_t;

extern parse_node *expr_eval(compile_ctx_t *ctx, parse_node *node, bool do_eval_dollar);
extern int compile(struct libasm80_as_desc_t *desc, dynarray *parse, jmp_buf *error_jmp_env);
extern void compile_instruction(compile_ctx_t *ctx, char *name, LIST *args);
extern void report_error(compile_ctx_t *ctx, char *fmt, ...);
extern void report_warning(compile_ctx_t *ctx, char *fmt, ...);
extern void register_fwd_lookup(compile_ctx_t *ctx,
                          parse_node *unresolved_node,
                          uint32_t pos,
                          int nbytes,
                          bool relative,
                          uint32_t instr_pc);

#pragma once

#include <setjmp.h>

#include "common.h"
#include "parse.h"
#include "buffer.h"
#include "libasm.h"

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
} rept_ctx_t;

typedef struct {
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
  rept_ctx_t *curr_rept;
  int lookup_rept_iter_id;
} compile_ctx_t;

typedef struct {
  parse_node *node;
  uint32_t pos;
  int nbytes;
  bool relative;
  uint32_t instr_pc;
  int section_id;
  int rept_iter_id;
} patch_t;

extern parse_node *expr_eval(compile_ctx_t *ctx, parse_node *node, bool do_eval_dollar);
extern int compile(struct libasm_as_desc_t *desc, dynarray *parse, jmp_buf *error_jmp_env);
extern void compile_instruction(compile_ctx_t *ctx, char *name, LIST *args);
extern void report_error(compile_ctx_t *ctx, char *fmt, ...);
extern void report_warning(compile_ctx_t *ctx, char *fmt, ...);
extern void register_fwd_lookup(compile_ctx_t *ctx,
                          parse_node *unresolved_node,
                          uint32_t pos,
                          int nbytes,
                          bool relative,
                          uint32_t instr_pc);

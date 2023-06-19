#pragma once

#include "common.h"
#include "buffer.h"

#define ASM_TARGET_RAW 0
#define ASM_TARGET_ELF 1

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
} compile_ctx_t;

typedef struct {
  parse_node *node;
  uint32_t pos;
  int nbytes;
  bool negate;
  bool was_reladdr;
  bool curr_pc_reladdr;
  int section_id;
} patch_t;

extern parse_node *expr_eval(compile_ctx_t *ctx, parse_node *node, bool do_eval_dollar);
extern void compile(dynarray *parse, hashmap *defineopts, dynarray *includeopts, char *outfile, int target);
extern void compile_instruction(compile_ctx_t *ctx, char *name, LIST *args);
extern void report_error(compile_ctx_t *ctx, char *fmt, ...);
extern void report_warning(compile_ctx_t *ctx, char *fmt, ...);
extern void register_fwd_lookup(compile_ctx_t *ctx,
                          parse_node *unresolved_node,
                          uint32_t pos,
                          int nbytes,
                          bool negate,
                          bool was_reladdr,
                          uint32_t curr_pc_reladdr);

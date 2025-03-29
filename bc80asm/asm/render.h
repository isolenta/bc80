#pragma once

#include "asm/compile.h"
#include "bits/dynarray.h"

static inline section_ctx_t *get_current_section(compile_ctx_t *ctx) {
  return (section_ctx_t *)dfirst(dynarray_nth_cell(ctx->sections, ctx->curr_section_id));
}

extern void render_byte(compile_ctx_t *ctx, char b, int cycles);
extern void render_2bytes(compile_ctx_t *ctx, char b1, char b2, int cycles);
extern void render_3bytes(compile_ctx_t *ctx, char b1, char b2, char b3, int cycles);
extern void render_4bytes(compile_ctx_t *ctx, char b1, char b2, char b3, char b4, int cycles);
extern void render_word(compile_ctx_t *ctx, int ival);
extern void render_bytes(compile_ctx_t *ctx, char *buf, uint32_t len);
extern void render_block(compile_ctx_t *ctx, char filler, uint32_t len);
extern void render_from_file(compile_ctx_t *ctx, char *filename, dynarray *includeopts);
extern void render_reorg(compile_ctx_t *ctx);
extern void render_patch(compile_ctx_t *ctx, patch_t *patch, int value);

extern void render_start(compile_ctx_t *ctx);
extern uint32_t render_finish(compile_ctx_t *ctx, char **dest_buf);
extern uint32_t render_sna(compile_ctx_t *ctx, char **dest_buf);
extern uint32_t render_elf(compile_ctx_t *ctx, char **dest_buf);
#pragma once

#include "compile.h"

extern void render_byte(compile_ctx_t *ctx, char b);
extern void render_2bytes(compile_ctx_t *ctx, char b1, char b2);
extern void render_3bytes(compile_ctx_t *ctx, char b1, char b2, char b3);
extern void render_4bytes(compile_ctx_t *ctx, char b1, char b2, char b3, char b4);
extern void render_word(compile_ctx_t *ctx, int ival);
extern void render_bytes(compile_ctx_t *ctx, char *buf, uint32_t len);
extern void render_block(compile_ctx_t *ctx, char filler, uint32_t len);
extern void render_from_file(compile_ctx_t *ctx, char *filename, dynarray *includeopts);
extern void render_start(compile_ctx_t *ctx);
extern void render_save(compile_ctx_t *ctx, char *filename);
extern void render_reorg(compile_ctx_t *ctx);
extern void render_patch(compile_ctx_t *ctx, patch_t *patch, int value);

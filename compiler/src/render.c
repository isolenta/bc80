#include <stdio.h>
#include <string.h>

#include "filesystem.h"
#include "common.h"
#include "compile.h"

void render_start(compile_ctx_t *ctx) {
  ctx->rawbin = buffer_init();
}

void render_save(compile_ctx_t *ctx, char *filename) {
  assert(ctx->rawbin);

  FILE *fp = fopen(filename, "w");
  if (!fp) {
    perror(filename);
    return;
  }

  size_t ret = fwrite(ctx->rawbin->data, ctx->rawbin->len, 1, fp);
  if (ret != 1) {
    perror(filename);
  }

  fclose(fp);
}

void render_byte(compile_ctx_t *ctx, char b) {
  assert(ctx->rawbin);
  buffer_append_char(ctx->rawbin, b);
  ctx->curr_pc += 1;
}

void render_2bytes(compile_ctx_t *ctx, char b1, char b2) {
  assert(ctx->rawbin);
  buffer_append_char(ctx->rawbin, b1);
  buffer_append_char(ctx->rawbin, b2);
  ctx->curr_pc += 2;
}

void render_3bytes(compile_ctx_t *ctx, char b1, char b2, char b3) {
  assert(ctx->rawbin);
  buffer_append_char(ctx->rawbin, b1);
  buffer_append_char(ctx->rawbin, b2);
  buffer_append_char(ctx->rawbin, b3);
  ctx->curr_pc += 3;
}

void render_4bytes(compile_ctx_t *ctx, char b1, char b2, char b3, char b4) {
  assert(ctx->rawbin);
  buffer_append_char(ctx->rawbin, b1);
  buffer_append_char(ctx->rawbin, b2);
  buffer_append_char(ctx->rawbin, b3);
  buffer_append_char(ctx->rawbin, b4);
  ctx->curr_pc += 4;
}

void render_word(compile_ctx_t *ctx, int ival) {
  assert(ctx->rawbin);

  if (ival != (ival & 0xFFFF))
    report_warning(ctx, "integer value %d was cropped to 16 bit size: %d applied\n", ival, ival & 0xFFFF);

  buffer_append_char(ctx->rawbin, (ival >> 8) & 0xff);
  buffer_append_char(ctx->rawbin, ival & 0xff);
  ctx->curr_pc += 2;
}

void render_bytes(compile_ctx_t *ctx, char *buf, uint32_t len) {
  assert(ctx->rawbin);
  buffer_append_binary(ctx->rawbin, buf, len);
  ctx->curr_pc += len;
}

void render_block(compile_ctx_t *ctx, char filler, uint32_t len) {
  assert(ctx->rawbin);

  char *tmp = malloc(len);
  memset(tmp, filler, len);
  buffer_append_binary(ctx->rawbin, tmp, len);
  free(tmp);

  ctx->curr_pc += len;
}

void render_from_file(compile_ctx_t *ctx, char *filename, dynarray *includeopts) {
  assert(ctx->rawbin);

  char *path = fs_abs_path(filename, includeopts);
  if (path == NULL) {
    report_error(ctx, "file not found: %s\n", filename);
    return;
  }

  size_t size = fs_file_size(path);
  char *buf = malloc(size);

  FILE *fp = fopen(path, "r");
  size_t ret = fread(buf, size, 1, fp);
  if (ret != 1)
    report_error(ctx, "unable to read %ld bytes from %s\n", size, filename);

  buffer_append_binary(ctx->rawbin, buf, size);
  free(buf);
  ctx->curr_pc += size;
}

void render_reorg(compile_ctx_t *ctx) {
  assert(ctx->rawbin);

  if (ctx->curr_pc <= ctx->rawbin->len) {
    // rewind: adjust buffer position and don't care about tail
    ctx->rawbin->len = ctx->curr_pc;
  } else {
    // enlarge buffer and fill up to new org
    size_t fill_size = ctx->curr_pc - ctx->rawbin->len + 1;
    char *tmp = malloc(fill_size);
    memset(tmp, 0, fill_size);
    buffer_append_binary(ctx->rawbin, tmp, fill_size);
    free(tmp);
  }
}

void render_patch(compile_ctx_t *ctx, patch_t *patch, int value) {
  assert(ctx->rawbin);
  assert((patch->pos + patch->nbytes - 1) < ctx->rawbin->len);

  if (patch->nbytes == 2) {
    ctx->rawbin->data[patch->pos] = value & 0xFF;
    ctx->rawbin->data[patch->pos + 1] = (value >> 8) & 0xFF;
  } else if (patch->nbytes == 1) {
    ctx->rawbin->data[patch->pos] = value & 0xFF;
  } else {
    report_error(ctx, "2nd pass: unable to patch %d bytes at once\n", patch->nbytes);
  }

  // TODO: negate, relative etc
}
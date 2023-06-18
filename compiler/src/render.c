#include <stdio.h>
#include <string.h>

#include "filesystem.h"
#include "common.h"
#include "compile.h"
#include "render.h"

void render_start(compile_ctx_t *ctx) {
  // declare default target section 'text'
  section_ctx_t *text = (section_ctx_t *)malloc(sizeof(section_ctx_t));

  text->curr_pc = 0;
  text->content = buffer_init();
  text->name = strdup("text");

  ctx->sections = dynarray_append_ptr(ctx->sections, text);
  ctx->curr_section_id = 0;
}

static void render_raw(compile_ctx_t *ctx, char *filename) {
  buffer *output = get_current_section(ctx)->content;

  FILE *fp = fopen(filename, "w");
  if (!fp) {
    perror(filename);
    return;
  }

  size_t ret = fwrite(output->data, output->len, 1, fp);
  if (ret != 1) {
    perror(filename);
  }

  fclose(fp);
}

static void render_elf(compile_ctx_t *ctx, char *filename) {
  report_error(ctx, "ELF target is not supported yet\n");
}

void render_save(compile_ctx_t *ctx, char *filename) {
  if (ctx->target == ASM_TARGET_RAW) {
    if (dynarray_length(ctx->sections) > 1)
      report_error(ctx, "raw target supports only single section but source have %d sections\n", dynarray_length(ctx->sections));

    render_raw(ctx, filename);
  } else if (ctx->target == ASM_TARGET_ELF) {
    render_elf(ctx, filename);
  } else {
    assert(0);
  }
}

void render_byte(compile_ctx_t *ctx, char b) {
  section_ctx_t *section = get_current_section(ctx);
  buffer_append_char(section->content, b);
  section->curr_pc += 1;
}

void render_2bytes(compile_ctx_t *ctx, char b1, char b2) {
  section_ctx_t *section = get_current_section(ctx);
  buffer_append_char(section->content, b1);
  buffer_append_char(section->content, b2);
  section->curr_pc += 2;
}

void render_3bytes(compile_ctx_t *ctx, char b1, char b2, char b3) {
  section_ctx_t *section = get_current_section(ctx);
  buffer_append_char(section->content, b1);
  buffer_append_char(section->content, b2);
  buffer_append_char(section->content, b3);
  section->curr_pc += 3;
}

void render_4bytes(compile_ctx_t *ctx, char b1, char b2, char b3, char b4) {
  section_ctx_t *section = get_current_section(ctx);
  buffer_append_char(section->content, b1);
  buffer_append_char(section->content, b2);
  buffer_append_char(section->content, b3);
  buffer_append_char(section->content, b4);
  section->curr_pc += 4;
}

void render_word(compile_ctx_t *ctx, int ival) {
  section_ctx_t *section = get_current_section(ctx);

  if (ival != (ival & 0xFFFF))
    report_warning(ctx, "integer value %d was cropped to 16 bit size: %d applied\n", ival, ival & 0xFFFF);

  buffer_append_char(section->content, (ival >> 8) & 0xff);
  buffer_append_char(section->content, ival & 0xff);
  section->curr_pc += 2;
}

void render_bytes(compile_ctx_t *ctx, char *buf, uint32_t len) {
  section_ctx_t *section = get_current_section(ctx);
  buffer_append_binary(section->content, buf, len);
  section->curr_pc += len;
}

void render_block(compile_ctx_t *ctx, char filler, uint32_t len) {
  section_ctx_t *section = get_current_section(ctx);

  char *tmp = malloc(len);
  memset(tmp, filler, len);
  buffer_append_binary(section->content, tmp, len);
  free(tmp);

  section->curr_pc += len;
}

void render_from_file(compile_ctx_t *ctx, char *filename, dynarray *includeopts) {
  section_ctx_t *section = get_current_section(ctx);

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

  buffer_append_binary(section->content, buf, size);
  free(buf);
  section->curr_pc += size;
}

void render_reorg(compile_ctx_t *ctx) {
  section_ctx_t *section = get_current_section(ctx);

  if (section->curr_pc <= section->content->len) {
    // rewind: adjust buffer position and don't care about tail
    section->content->len = section->curr_pc;
  } else {
    // enlarge buffer and fill up to new org
    size_t fill_size = section->curr_pc - section->content->len + 1;
    char *tmp = malloc(fill_size);
    memset(tmp, 0, fill_size);
    buffer_append_binary(section->content, tmp, fill_size);
    free(tmp);
  }
}

void render_patch(compile_ctx_t *ctx, patch_t *patch, int value) {
  section_ctx_t *section = get_current_section(ctx);
  assert((patch->pos + patch->nbytes - 1) < section->content->len);

  if (patch->nbytes == 2) {
    section->content->data[patch->pos] = value & 0xFF;
    section->content->data[patch->pos + 1] = (value >> 8) & 0xFF;
  } else if (patch->nbytes == 1) {
    section->content->data[patch->pos] = value & 0xFF;
  } else {
    report_error(ctx, "2nd pass: unable to patch %d bytes at once\n", patch->nbytes);
  }

  // TODO: negate, relative etc
}
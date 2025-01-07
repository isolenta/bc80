#include <stdio.h>
#include <string.h>

#include "asm/render.h"
#include "common/buffer.h"
#include "common/filesystem.h"

#define DEFAULT_SECTION_NAME ".text"

void render_start(compile_ctx_t *ctx) {
  section_ctx_t *defsect = (section_ctx_t *)xmalloc(sizeof(section_ctx_t));

  defsect->start = 0;
  defsect->curr_pc = 0;
  defsect->content = buffer_init();
  defsect->name = xstrdup(DEFAULT_SECTION_NAME);
  defsect->filler = 0;

  ctx->sections = dynarray_append_ptr(ctx->sections, defsect);
  ctx->curr_section_id = 0;
}

static uint32_t render_raw(compile_ctx_t *ctx, char **dest_buf) {
  buffer *output = get_current_section(ctx)->content;
  *dest_buf = buffer_dup(output);
  return output->len;
}

uint32_t render_finish(compile_ctx_t *ctx, char **dest_buf) {
  assert(dest_buf);

  if (ctx->target == ASM_TARGET_RAW) {
    if (dynarray_length(ctx->sections) > 1)
      report_error(ctx, "raw target supports only single section but source has %d sections", dynarray_length(ctx->sections));

    return render_raw(ctx, dest_buf);
  } else if (ctx->target == ASM_TARGET_ELF) {
    return render_elf(ctx, dest_buf);
  } else if (ctx->target == ASM_TARGET_SNA) {
    return render_sna(ctx, dest_buf);
  } else {
    report_error(ctx, "unsupported target %d", ctx->target);
  }

  return 0;
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

  buffer_append_char(section->content, ival & 0xff);
  buffer_append_char(section->content, (ival >> 8) & 0xff);
  section->curr_pc += 2;
}

void render_bytes(compile_ctx_t *ctx, char *buf, uint32_t len) {
  section_ctx_t *section = get_current_section(ctx);
  buffer_append_binary(section->content, buf, len);
  section->curr_pc += len;
}

void render_block(compile_ctx_t *ctx, char filler, uint32_t len) {
  section_ctx_t *section = get_current_section(ctx);

  char *tmp = xmalloc(len);
  memset(tmp, filler, len);
  buffer_append_binary(section->content, tmp, len);
  xfree(tmp);

  section->curr_pc += len;
}

void render_from_file(compile_ctx_t *ctx, char *filename, dynarray *includeopts) {
  section_ctx_t *section = get_current_section(ctx);

  char *path = fs_abs_path(filename, includeopts);
  if (path == NULL) {
    report_error(ctx, "file not found: %s", filename);
    return;
  }

  size_t size = fs_file_size(path);
  char *buf = xmalloc(size);

  FILE *fp = fopen(path, "r");
  size_t ret = fread(buf, size, 1, fp);
  if (ret != 1)
    report_error(ctx, "unable to read %ld bytes from %s", size, filename);

  buffer_append_binary(section->content, buf, size);
  xfree(buf);
  section->curr_pc += size;
}

void render_reorg(compile_ctx_t *ctx) {
  section_ctx_t *section = get_current_section(ctx);

  if ((section->curr_pc - section->start) <= section->content->len) {
    // rewind: we already adjusted buffer position and for now we do nothing.
    // We still have a tail with previously generated code but leave it as is: will overwrite it eventually
  } else {
    // enlarge buffer and fill up to new org
    size_t fill_size = section->curr_pc - section->start - section->content->len;
    char *tmp = xmalloc(fill_size);
    memset(tmp, section->filler, fill_size);
    buffer_append_binary(section->content, tmp, fill_size);
    xfree(tmp);
  }
}

void render_patch(compile_ctx_t *ctx, patch_t *patch, int value) {
  section_ctx_t *section = (section_ctx_t *)dfirst(dynarray_nth_cell(ctx->sections, patch->section_id));

  assert((patch->pos + patch->nbytes - 1) < section->content->maxlen);

  if (patch->relative) {
    value -= patch->instr_pc;
  }

  if (patch->nbytes == 2) {
    section->content->data[patch->pos] = value & 0xFF;
    section->content->data[patch->pos + 1] = (value >> 8) & 0xFF;
  } else if (patch->nbytes == 1) {
    section->content->data[patch->pos] = value & 0xFF;
  } else {
    report_error(ctx, "2nd pass: unable to patch %d bytes at once", patch->nbytes);
  }
}
#include <stdio.h>
#include <string.h>

#include "filesystem.h"
#include "common.h"
#include "compile.h"
#include "render.h"
#include "elf.h"

#define SHSTRTAB_NAME ".shstrtab"
#define DEFAULT_SECTION_NAME ".text"

void render_start(compile_ctx_t *ctx) {
  section_ctx_t *defsect = (section_ctx_t *)malloc(sizeof(section_ctx_t));

  defsect->start = 0;
  defsect->curr_pc = 0;
  defsect->content = buffer_init();
  defsect->name = strdup(DEFAULT_SECTION_NAME);
  defsect->filler = 0;

  ctx->sections = dynarray_append_ptr(ctx->sections, defsect);
  ctx->curr_section_id = 0;
}

static uint32_t render_raw(compile_ctx_t *ctx, char **dest_buf) {
  buffer *output = get_current_section(ctx)->content;
  *dest_buf = buffer_dup(output);
  return output->len;
}

static uint32_t render_elf(compile_ctx_t *ctx, char **dest_buf) {
  dynarray_cell *dc;
  uint32_t elfsize = 0;

  // add section to store section names (now it's empty, will populate it on the fly)
  section_ctx_t *shstrtab = (section_ctx_t *)malloc(sizeof(section_ctx_t));
  shstrtab->content = buffer_init();
  shstrtab->name = strdup(SHSTRTAB_NAME);
  buffer_append_char(shstrtab->content, '\0');  // first byte in strtab is always zero
  ctx->sections = dynarray_append_ptr(ctx->sections, shstrtab);

  // start building elf file
  buffer *elf = buffer_init();
  int elf_fh_offset = buffer_reserve(elf, sizeof(elf_file_header));
  elf_file_header *elf_fh = (elf_file_header *)&elf->data[elf_fh_offset];

  elf_fh->ei_magic[0] = 0x7f;
  elf_fh->ei_magic[1] = 0x45;
  elf_fh->ei_magic[2] = 0x4c;
  elf_fh->ei_magic[3] = 0x46;

  elf_fh->ei_class = ELFCLASS32;
  elf_fh->ei_data = ELFDATA2LSB;
  elf_fh->ei_version = EIV_CURRENT;
  elf_fh->ei_osabi = ELFOSABI_NONE;
  elf_fh->ei_abiversion = 0;

  elf_fh->e_type = ET_REL;
  elf_fh->e_machine = EM_NONE;
  elf_fh->e_version = EV_CURRENT;

  elf_fh->e_entry = 0;
  elf_fh->e_phoff = 0;
  elf_fh->e_shoff = elf->len; // section headers follow right after elf_file_header
  elf_fh->e_flags = 0;
  elf_fh->e_ehsize = sizeof(elf_file_header);
  elf_fh->e_phentsize = 32;
  elf_fh->e_phnum = 0;
  elf_fh->e_shentsize = sizeof(elf_section_header);
  elf_fh->e_shnum = dynarray_length(ctx->sections);
  elf_fh->e_shstrndx = dynarray_length(ctx->sections) - 1; // .shstrtab is always last section

  elf_section_header **sech_ptrs = (elf_section_header **)malloc(sizeof(elf_section_header *) * dynarray_length(ctx->sections));

  int i = 0;
  foreach (dc, ctx->sections) {
    section_ctx_t *section = (section_ctx_t *)dfirst(dc);

    int sech_offset = buffer_reserve(elf, sizeof(elf_section_header));
    elf_section_header *sech = (elf_section_header *)&elf->data[sech_offset];

    // add section name to .shstrtab
    int str_offset = buffer_append(shstrtab->content, section->name);
    buffer_append_char(shstrtab->content, '\0');
    sech->sh_name = str_offset;

    if (strcmp(section->name, SHSTRTAB_NAME) == 0) {
      sech->sh_type = SHT_STRTAB;
      sech->sh_flags = 0;
      sech->sh_addr = 0;
    } else {
      sech->sh_type = SHT_PROGBITS;
      sech->sh_flags = SHF_ALLOC | SHF_EXECINSTR | SHF_WRITE;

      if (section->start == -1)
        report_error(ctx, "section start address for %s isn't defined (no ORG directive)", section->name);

      sech->sh_addr = section->start;
    }

    sech->sh_link = 0;
    sech->sh_info = 0;
    sech->sh_addralign = 0;
    sech->sh_entsize = 0;
    sech->sh_size = section->content->len;

    // remember pointer to section header to fill offset of section data later
    sech_ptrs[i++] = sech;
  }

  // add sections data
  i = 0;
  foreach (dc, ctx->sections) {
    section_ctx_t *section = (section_ctx_t *)dfirst(dc);
    sech_ptrs[i++]->sh_offset = buffer_append_binary(elf, section->content->data, section->content->len);
  }

  free(sech_ptrs);

  *dest_buf = buffer_dup(elf);
  elfsize = elf->len;
  buffer_free(elf);

  return elfsize;
}

uint32_t render_finish(compile_ctx_t *ctx, char **dest_buf) {
  assert(dest_buf);

  if (ctx->target == ASM_TARGET_RAW) {
    if (dynarray_length(ctx->sections) > 1)
      report_error(ctx, "raw target supports only single section but source has %d sections", dynarray_length(ctx->sections));

    return render_raw(ctx, dest_buf);
  } else if (ctx->target == ASM_TARGET_ELF) {
    return render_elf(ctx, dest_buf);
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

  if (ival != (ival & 0xFFFF))
    report_warning(ctx, "integer value %d was cropped to 16 bit size: %d applied\n", ival, ival & 0xFFFF);

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
    report_error(ctx, "file not found: %s", filename);
    return;
  }

  size_t size = fs_file_size(path);
  char *buf = malloc(size);

  FILE *fp = fopen(path, "r");
  size_t ret = fread(buf, size, 1, fp);
  if (ret != 1)
    report_error(ctx, "unable to read %ld bytes from %s", size, filename);

  buffer_append_binary(section->content, buf, size);
  free(buf);
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
    char *tmp = malloc(fill_size);
    memset(tmp, section->filler, fill_size);
    buffer_append_binary(section->content, tmp, fill_size);
    free(tmp);
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
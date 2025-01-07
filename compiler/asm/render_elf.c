#include <stdio.h>
#include <string.h>

#include "asm/elf.h"
#include "asm/render.h"
#include "common/buffer.h"

#define SHSTRTAB_NAME ".shstrtab"

uint32_t render_elf(compile_ctx_t *ctx, char **dest_buf) {
  dynarray_cell *dc;
  uint32_t elfsize = 0;

  // add section to store section names (now it's empty, will populate it on the fly)
  section_ctx_t *shstrtab = (section_ctx_t *)xmalloc(sizeof(section_ctx_t));
  shstrtab->content = buffer_init();
  shstrtab->name = xstrdup(SHSTRTAB_NAME);
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

  elf_section_header **sech_ptrs = (elf_section_header **)xmalloc(sizeof(elf_section_header *) * dynarray_length(ctx->sections));

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

  xfree(sech_ptrs);

  *dest_buf = buffer_dup(elf);
  elfsize = elf->len;
  buffer_free(elf);

  return elfsize;
}

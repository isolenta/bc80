#pragma once

#define EI_NIDENT 16

typedef struct {
    uint8_t       ei_magic[4];

#define ELFCLASS32 1
    uint8_t       ei_class;

#define ELFDATA2LSB 1
    uint8_t       ei_data;

#define EIV_CURRENT 1
    uint8_t       ei_version;

#define ELFOSABI_NONE 0
    uint8_t       ei_osabi;
    uint8_t       ei_abiversion;
    uint8_t       ei_pad[7];

#define ET_REL 1
    uint16_t      e_type;

#define EM_NONE 0
    uint16_t      e_machine;

#define EV_CURRENT 1
    uint32_t      e_version;

    uint32_t      e_entry;
    uint32_t      e_phoff;
    uint32_t      e_shoff;
    uint32_t      e_flags;
    uint16_t      e_ehsize;
    uint16_t      e_phentsize;
    uint16_t      e_phnum;
    uint16_t      e_shentsize;
    uint16_t      e_shnum;
    uint16_t      e_shstrndx;
} elf_file_header;

typedef struct {
    uint32_t   sh_name;

#define SHT_NULL 0
#define SHT_PROGBITS 1
#define SHT_SYMTAB 2
#define SHT_STRTAB 3
#define SHT_RELA 4
#define SHT_HASH 5
#define SHT_DYNAMIC 6
#define SHT_NOTE 7
#define SHT_NOBITS 8
#define SHT_REL 9
#define SHT_SHLIB 10
#define SHT_DYNSYM 11
#define SHT_INIT_ARRAY 14
#define SHT_FINI_ARRAY 15
#define SHT_PREINIT_ARRAY 16
#define SHT_GROUP 17
#define SHT_SYMTAB_SHNDX 18
    uint32_t   sh_type;

#define SHF_WRITE 0x1
#define SHF_ALLOC 0x2
#define SHF_EXECINSTR 0x4
#define SHF_MERGE 0x10
#define SHF_STRINGS 0x20
#define SHF_INFO_LINK 0x40
#define SHF_LINK_ORDER 0x80
#define SHF_OS_NONCONFORMING 0x100
#define SHF_GROUP 0x200
#define SHF_TLS 0x400
#define SHF_COMPRESSED 0x800
    uint32_t   sh_flags;

    uint32_t   sh_addr;
    uint32_t   sh_offset;
    uint32_t   sh_size;
    uint32_t   sh_link;
    uint32_t   sh_info;
    uint32_t   sh_addralign;
    uint32_t   sh_entsize;
} elf_section_header;

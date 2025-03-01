#pragma once

#include <stdbool.h>
#include <stdint.h>

#include "common/error.h"

typedef struct dynarray dynarray;
typedef struct hashmap hashmap;

#define ASM_TARGET_RAW 0
#define ASM_TARGET_ELF 1
#define ASM_TARGET_SNA 2

enum PROFILE_MODE {
  PROFILE_NONE = 0,
  PROFILE_GLOBALS,
  PROFILE_ALL,
};

struct libasm_as_desc_t {
  char *source;                       // source text to assembly
  char *filename;                     // name of source file (for error reporing only)
  dynarray *includeopts;              // array of directories where it should search files for 'include' and 'incbin' directives
  hashmap *defineopts;                // map (key-value) of predefined constants. Used to prepopulate symtab (the same as EQU expressions)
  int target;                         // output format, one of ASM_TARGET_RAW, ASM_TARGET_ELF, ASM_TARGET_SNA

  // options for ASM_TARGET_SNA
  bool sna_generic;                   // use generic device (don't initialize RAM areas like UDG and SYSVARS)
  int sna_pc_addr;                    // initial PC value for ASM_TARGET_SNA (-1 if argument omitted)
  int sna_ramtop;                     // RAM top address (suitable for user programs)

  int profile_mode;                   // how to perform auto profile: for all blocks, only global labels or never
  bool profile_data;                  // display profile information for no-code blocks

  char **dest;                        // libasm_as() allocates buffer to store compilation output and places its pointer here. Caller is reposible to free this memory.
  uint32_t *dest_size;                // size of destination buffer
};

struct libasm_disas_desc_t {
  char *data;                         // source binary data to disassemble
  uint32_t data_size;                 // source binary data size
  bool opt_addr;                      // add address to output (as comment)
  bool opt_source;                    // add hexdump of opcode (as comment)
  bool opt_labels;                    // add text labels instead of addresses (for jumps and calls)
  bool opt_timings;                   // add number of cycles per instruction
  int  org;                           // start address
};

// compile source text provided by desc.
// return 0 in success case, non-zero otherwise
extern int libasm_as(struct libasm_as_desc_t *desc);

// disassemble binary data provided by desc.
// return asciiz string with disassemble; caller is responsible to free this pointer
extern char *libasm_disas(struct libasm_disas_desc_t *desc);

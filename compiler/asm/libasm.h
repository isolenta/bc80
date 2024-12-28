#pragma once

#include <stdint.h>
#include <stdbool.h>

typedef struct dynarray dynarray;
typedef struct hashmap hashmap;

#define ASM_TARGET_RAW 0
#define ASM_TARGET_ELF 1
#define ASM_TARGET_SNA 2

// user-defined callback for error processing:
// message: short error message
// detail: verbose error message add-in (can be NULL)
// line: error line position in the source text
// return value:
//   0: continue processing (can be dangerous, compilation state can be corrupted after error)
//   non zero: stop processing and return this code as result of libasm_as()
typedef int (*error_callback_type) (const char *message, const char *filename, int line);

// the same as error_callback_type but for non-critical warnings
typedef void (*warning_callback_type) (const char *message, const char *filename, int line);

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

  error_callback_type error_cb;       // user-defined callback for error processing (see above)
  warning_callback_type warning_cb;   // user-defined callback for warning processing (see above)

  char **dest;                        // libasm_as() allocates buffer to store compilation output and places its pointer here. Caller is reposible to free this memory.
  uint32_t *dest_size;                // size of destination buffer
};

struct libasm_disas_desc_t {
  char *data;                         // source binary data to disassemble
  uint32_t data_size;                 // source binary data size
  bool opt_addr;                      // add address to output (as comment)
  bool opt_source;                    // add hexdump of opcode (as comment)
  bool opt_labels;                    // add text labels instead of addresses (for jumps and calls)
  int  org;                           // start address
};

// compile source text provided by desc.
// return 0 in success case, non-zero otherwise
extern int libasm_as(struct libasm_as_desc_t *desc);

// disassemble binary data provided by desc.
// return asciiz string with disassemble; caller is responsible to free this pointer
extern char *libasm_disas(struct libasm_disas_desc_t *desc);

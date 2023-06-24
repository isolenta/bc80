#pragma once

#include <stdint.h>

typedef struct dynarray dynarray;
typedef struct hashmap hashmap;

#define ASM_TARGET_RAW 0
#define ASM_TARGET_ELF 1

// user-defined callback for error processing:
// message: short error message
// detail: verbose error message add-in (can be NULL)
// line: error line position in the source text
// return value:
//   0: continue processing (can be dangerous, compilation state can be corrupted after error)
//   non zero: stop processing and return this code as result of libasm80_as()
typedef int (*error_callback_type) (const char *message, int line);

// the same as error_callback_type but for non-critical warnings
typedef void (*warning_callback_type) (const char *message, int line);

struct libasm80_as_desc_t {
  char *source;                       // source text to assembly
  dynarray *includeopts;              // array of directories where it should search files for 'include' and 'incbin' directives
  hashmap *defineopts;                // map (key-value) of predefined constants. Used to prepopulate symtab (the same as EQU expressions)
  int target;                         // output format, one of ASM_TARGET_RAW, ASM_TARGET_ELF

  error_callback_type error_cb;       // user-defined callback for error processing (see above)
  warning_callback_type warning_cb;   // user-defined callback for warning processing (see above)

  char **dest;                        // libasm80_as() allocates buffer to store compilation output and places its pointer here. Caller is reposible to free this memory.
  uint32_t *dest_size;                // size of destination buffer
};

struct libasm80_disas_desc_t {
  char *data;                         // source binary data to disassemble
  uint32_t data_size;                 // source binary data size
  bool opt_addr;                      // add address to output (as comment)
  bool opt_source;                    // add hexdump of opcode (as comment)
  bool opt_labels;                    // add text labels instead of addresses (for jumps and calls)
  int  org;                           // start address
};

// compile source text provided by desc.
// return 0 in success case, non-zero otherwise
extern int libasm80_as(struct libasm80_as_desc_t *desc);

// disassemble binary data provided by desc.
// return asciiz string with disassemble; caller is responsible to free this pointer
extern char *libasm80_disas(struct libasm80_disas_desc_t *desc);

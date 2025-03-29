#pragma once

#include <stdbool.h>

typedef struct dynarray dynarray;

enum {
  ASM_TARGET_RAW = 0,
  ASM_TARGET_ELF,
  ASM_TARGET_SNA,
};

enum {
  PROFILE_NONE = 0,
  PROFILE_GLOBALS,
  PROFILE_ALL,
};

extern dynarray *g_includeopts;

typedef struct {
  int target;                         // output format: one of ASM_TARGET_RAW, ASM_TARGET_ELF, ASM_TARGET_SNA

  // options for ASM_TARGET_SNA
  bool sna_generic;                   // use generic device (don't initialize RAM areas like UDG and SYSVARS)
  int sna_pc_addr;                    // initial PC value for ASM_TARGET_SNA (-1 if argument omitted)
  int sna_ramtop;                     // RAM top address (suitable for user programs)

  int profile_mode;                   // how to perform auto profile: for all blocks, only global labels or never
  bool profile_data;                  // display profile information for no-code blocks
} compile_opts;

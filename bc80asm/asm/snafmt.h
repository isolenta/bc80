#pragma once

struct snafmt_header {
  uint8_t i;
  uint16_t hl_sh;
  uint16_t de_sh;
  uint16_t bc_sh;
  uint16_t af_sh;
  uint16_t hl;
  uint16_t de;
  uint16_t bc;
  uint16_t iy;
  uint16_t ix;
  uint8_t iff2;
  uint8_t r;
  uint16_t af;
  uint16_t sp;
  uint8_t im;
  uint8_t border;
} __attribute__ ((__packed__));

_Static_assert (sizeof(struct snafmt_header) == 27, "snafmt_header must have length 27 bytes");

#define SNA_SNAPSHOT_RAM_SIZE (48 * 1024)

// constant data offset
#define SNA_SNAPSHOT_ROM_SIZE (16 * 1024)

#define ZX_DEFAULT_RAMTOP (0x5D5B)

// for generic device it's actually not ramtop but just temporary stack pointer (stores snapshot PC)
#define SNA_DEFAULT_RAMTOP (0xFFF0)
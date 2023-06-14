#include <stdio.h>
#include <string.h>
#include <unistd.h>

#include "filesystem.h"
#include "disas.h"

char *MnemonicStrings[] = {
  GEN_INSTR_LIST(FN_STRING)
};

static void append_node(disas_context_t *ctx, disas_node_t *node) {
  if (ctx->nodes == NULL) {
    ctx->nodes = node;
    ctx->last_node = node;
  } else {
    ctx->last_node->next = node;
    ctx->last_node = node;
  }
}

static int add_instr0(disas_context_t *ctx, MnemonicEnum instr, int isize) {
  assert(isize <= 4);
  disas_node_t *node = (disas_node_t *)malloc(sizeof(disas_node_t));
  node->instr = instr;
  node->isize = isize;
  node->valid = true;
  node->addr = ctx->curr_addr;
  node->num_args = 0;
  ctx->curr_addr += node->isize;
  node->next = NULL;

  append_node(ctx, node);
  return node->isize;
}

static int add_instr1(disas_context_t *ctx, MnemonicEnum instr, int isize, disas_arg_t *arg1) {
  assert(isize <= 4);
  disas_node_t *node = (disas_node_t *)malloc(sizeof(disas_node_t));
  node->instr = instr;
  node->isize = isize;
  node->valid = true;
  node->addr = ctx->curr_addr;
  node->num_args = 1;
  node->args[0] = *arg1;
  ctx->curr_addr += node->isize;
  node->next = NULL;

  append_node(ctx, node);
  return node->isize;
}

static int add_instr2(disas_context_t *ctx, MnemonicEnum instr, int isize, disas_arg_t *arg1, disas_arg_t *arg2) {
  assert(isize <= 4);
  disas_node_t *node = (disas_node_t *)malloc(sizeof(disas_node_t));
  node->instr = instr;
  node->isize = isize;
  node->valid = true;
  node->addr = ctx->curr_addr;
  node->num_args = 2;
  node->args[0] = *arg1;
  node->args[1] = *arg2;
  ctx->curr_addr += node->isize;
  node->next = NULL;

  append_node(ctx, node);
  return node->isize;
}

static int add_invalid_instr(disas_context_t *ctx, uint8_t opcode) {
  disas_node_t *node = (disas_node_t *)malloc(sizeof(disas_node_t));
  node->instr = opcode;
  node->isize = 1;
  node->addr = ctx->curr_addr;
  node->valid = false;
  ctx->curr_addr += node->isize;
  node->next = NULL;

  append_node(ctx, node);
  return node->isize;
}

static disas_arg_t *mk_arg(arg_kind kind, int value, int extra, bool is_ref) {
  disas_arg_t *arg = (disas_arg_t *)malloc(sizeof(disas_arg_t));
  arg->kind = kind;
  arg->is_ref = is_ref;
  arg->value = value;
  arg->extra = extra;
  return arg;
}

static int process_ld(disas_context_t *ctx, uint8_t *data) {
  // special rule to avoid conflict with HALT instruction
  if (data[0] == 0x76)
    return 0;

  if (data[0] == 0x0a) {
    return add_instr2(ctx, LD, 1, mk_arg_A, mk_arg_BC);
  } else if (data[0] == 0x1a) {
    return add_instr2(ctx, LD, 1, mk_arg_A, mk_arg_DE);
  } else if (data[0] == 0xed && data[1] == 0x57) {
    return add_instr2(ctx, LD, 2, mk_arg_A, mk_arg_I);
  } else if (data[0] == 0xed && data[1] == 0x5f) {
    return add_instr2(ctx, LD, 2, mk_arg_A, mk_arg_R);
  } else if (data[0] == 0xed && data[1] == 0x47) {
    return add_instr2(ctx, LD, 2, mk_arg_I, mk_arg_A);
  } else if (data[0] == 0xed && data[1] == 0x4f) {
    return add_instr2(ctx, LD, 2, mk_arg_R, mk_arg_A);
  } else if (data[0] == 0x32) {
    return add_instr2(ctx, LD, 3, mk_arg(ARG_16IMM, data[1], data[2], true), mk_arg_A);
  } else if (data[0] == 0x3a) {
    return add_instr2(ctx, LD, 3, mk_arg_A, mk_arg(ARG_16IMM, data[1], data[2], true));
  } else if (data[0] == 0x36) {
    return add_instr2(ctx, LD, 2, mk_arg_HL_REF, mk_arg(ARG_8IMM, data[1], 0, false));
  } else if ((data[0] == 0xdd) || (data[0] == 0xfd)) {
    // indexed operand
    if (data[1] == 0x36) {
      // indexed destination with immediate source
      return add_instr2(ctx, LD, 4, mk_arg(ARG_INDEX, data[0] & 0x20, data[2], true), mk_arg(ARG_8IMM, data[3], 0, false));
    } else if ((data[1] & 0xf8) == 0x70) {
      return add_instr2(ctx, LD, 3, mk_arg(ARG_INDEX, data[0] & 0x20, data[2], true), mk_GPR(data[1] & 0x7));
    } else if ((data[1] & 0xc7) == 0x46) {
      return add_instr2(ctx, LD, 3, mk_GPR((data[1] >> 3) & 0x7), mk_arg(ARG_INDEX, data[0] & 0x20, data[2], true));
    } else if (data[1] == 0x21) {
      return add_instr2(ctx, LD, 4, mk_arg(ARG_REGPAIR_I, (data[0] & 0x20) ? REG_IY : REG_IX, 0, false), mk_arg(ARG_16IMM, data[2], data[3], false));
    } else if (data[1] == 0x2a) {
      return add_instr2(ctx, LD, 4, mk_arg(ARG_REGPAIR_I, (data[0] & 0x20) ? REG_IY : REG_IX, 0, false), mk_arg(ARG_16IMM, data[2], data[3], true));
    } else if (data[1] == 0x22) {
      return add_instr2(ctx, LD, 4, mk_arg(ARG_16IMM, data[2], data[3], true), mk_arg(ARG_REGPAIR_I, (data[0] & 0x20) ? REG_IY : REG_IX, 0, false));
    } else if (data[1] == 0xf9) {
      return add_instr2(ctx, LD, 2, mk_arg_SP, mk_arg(ARG_REGPAIR_I, (data[0] & 0x20) ? REG_IY : REG_IX, 0, false));
    }
  } else if ((data[0] & 0xf8) == 0x70) {
    return add_instr2(ctx, LD, 1, mk_arg_HL_REF, mk_GPR(data[0] & 0x7));
  } else if ((data[0] & 0xc7) == 0x46) {
    return add_instr2(ctx, LD, 1, mk_GPR((data[0] >> 3) & 0x7), mk_arg_HL_REF);
  } else if ((data[0] & 0xc0) == 0x40) {
    return add_instr2(ctx, LD, 1, mk_GPR((data[0] >> 3) & 0x7), mk_GPR(data[0] & 0x7));
  } else if ((data[0] & 0xc7) == 0x06) {
    return add_instr2(ctx, LD, 2, mk_GPR((data[0] >> 3) & 0x7), mk_arg(ARG_8IMM, data[1], 0, false));
  } else if (data[0] == 0x02) {
    return add_instr2(ctx, LD, 1, mk_arg(ARG_REGPAIR_P, REG_BC, 0, true), mk_arg_A);
  } else if (data[0] == 0x12) {
    return add_instr2(ctx, LD, 1, mk_arg(ARG_REGPAIR_P, REG_DE, 0, true), mk_arg_A);
  } else if ((data[0] & 0xcf) == 0x1) {
    return add_instr2(ctx, LD, 3, mk_arg(ARG_REGPAIR_Q, (data[0] >> 4) & 0x3, 0, false), mk_arg(ARG_16IMM, data[1], data[2], false));
  } else if (data[0] == 0xed) {
    if ((data[1] & 0xcf) == 0x43) {
      return add_instr2(ctx, LD, 4, mk_arg(ARG_16IMM, data[2], data[3], true), mk_arg(ARG_REGPAIR_Q, (data[1] >> 4) & 0x3, 0, false));
    } else if ((data[1] & 0xcf) == 0x4b) {
      return add_instr2(ctx, LD, 4, mk_arg(ARG_REGPAIR_Q, (data[1] >> 4) & 0x3, 0, false), mk_arg(ARG_16IMM, data[2], data[3], true));
    }
  } else if (data[0] == 0x21) {
    return add_instr2(ctx, LD, 3, mk_arg_HL, mk_arg(ARG_16IMM, data[1], data[2], false));
  } else if (data[0] == 0x2a) {
    return add_instr2(ctx, LD, 3, mk_arg_HL, mk_arg(ARG_16IMM, data[1], data[2], true));
  } else if (data[0] == 0x22) {
    return add_instr2(ctx, LD, 3, mk_arg(ARG_16IMM, data[1], data[2], true), mk_arg_HL);
  } else if (data[0] == 0xf9) {
    return add_instr2(ctx, LD, 1, mk_arg_SP, mk_arg_HL);
  }

  return 0;
}

static int process_pushpop(disas_context_t *ctx, uint8_t *data) {
  if ((data[0] & 0xcf) == 0xc5) {
    return add_instr1(ctx, PUSH, 1, mk_arg(ARG_REGPAIR_P, (data[0] >> 4) & 0x3, 0, false));
  } else if ((data[1] == 0xe5) && (data[0] == 0xdd || data[0] == 0xfd)) {
    return add_instr1(ctx, PUSH, 1, mk_arg(ARG_REGPAIR_I, (data[0] & 0x20) ? REG_IY : REG_IX, 0, false));
  } else if ((data[0] & 0xcf) == 0xc1) {
    return add_instr1(ctx, POP, 1, mk_arg(ARG_REGPAIR_P, (data[0] >> 4) & 0x3, 0, false));
  } else if ((data[1] == 0xe1) && (data[0] == 0xdd || data[0] == 0xfd)) {
    return add_instr1(ctx, POP, 1, mk_arg(ARG_REGPAIR_I, (data[0] & 0x20) ? REG_IY : REG_IX, 0, false));
  }

  return 0;
}

static int process_ex_exx(disas_context_t *ctx, uint8_t *data) {
  if (data[0] == 0x08) {
    return add_instr2(ctx, EX, 1, mk_arg(ARG_REGPAIR_P, 3, 0, false), mk_arg(ARG_AF_SHADOW, 0, 0, false));
  } else if (data[0] == 0xd9) {
    return add_instr0(ctx, EXX, 1);
  } else if (data[0] == 0xeb) {
    return add_instr2(ctx, EX, 1, mk_arg_DE, mk_arg_HL);
  } else if (data[0] == 0xe3) {
    return add_instr2(ctx, EX, 1, mk_arg(ARG_REGPAIR_Q, REG_SP, 0, true), mk_arg_HL);
  } else if (((data[0] == 0xdd) || (data[0] == 0xfd)) && (data[1] == 0xe3)) {
    return add_instr2(ctx, EX, 2, mk_arg(ARG_REGPAIR_Q, REG_SP, 0, true), mk_arg(ARG_REGPAIR_I, (data[0] & 0x20) ? REG_IY : REG_IX, 0, false));
  }

  return 0;
}

static int process_block_transfer_search(disas_context_t *ctx, uint8_t *data) {
  if (data[0] != 0xed)
    return 0;

  if (data[1] == 0xa0) {
    return add_instr0(ctx, LDI, 2);
  } else if (data[1] == 0xb0) {
    return add_instr0(ctx, LDIR, 2);
  } else if (data[1] == 0xa8) {
    return add_instr0(ctx, LDD, 2);
  } else if (data[1] == 0xb8) {
    return add_instr0(ctx, LDDR, 2);
  } else if (data[1] == 0xa1) {
    return add_instr0(ctx, CPI, 2);
  } else if (data[1] == 0xb1) {
    return add_instr0(ctx, CPIR, 2);
  } else if (data[1] == 0xa9) {
    return add_instr0(ctx, CPD, 2);
  } else if (data[1] == 0xb9) {
    return add_instr0(ctx, CPDR, 2);
  }

  return 0;
}

static int process_arithmetic(disas_context_t *ctx, uint8_t *data) {
  if (data[0] == 0x86) {
    return add_instr2(ctx, ADD, 1, mk_arg_A, mk_arg_HL_REF);
  } else if ((data[0] & 0xf8) == 0x80) {
    return add_instr2(ctx, ADD, 1, mk_arg_A, mk_GPR(data[0] & 0x7));
  } else if (((data[0] == 0xdd) || (data[0] == 0xfd)) && (data[1] == 0x86)) {
    return add_instr2(ctx, ADD, 3, mk_arg_A, mk_arg(ARG_INDEX, data[0] & 0x20, data[2], true));
  } else if (data[0] == 0xc6) {
    return add_instr2(ctx, ADD, 2, mk_arg_A, mk_arg(ARG_8IMM, data[1], 0, false));
  } else if ((data[0] & 0xcf) == 0x09) {
    return add_instr2(ctx, ADD, 1, mk_arg_HL, mk_arg(ARG_REGPAIR_Q, (data[0] >> 4) & 0x3, 0, false));
  } else if (((data[0] == 0xdd) || (data[0] == 0xfd)) && ((data[1] & 0xcf) == 0x09)) {
    return add_instr2(ctx, ADD, 2, mk_arg(ARG_REGPAIR_I, (data[0] & 0x20) ? REG_IY : REG_IX, 0, false), mk_arg(ARG_REGPAIR_Q, (data[1] >> 4) & 0x3, 0, false));
  } else if (data[0] == 0x8e) {
    return add_instr2(ctx, ADC, 1, mk_arg_A, mk_arg_HL_REF);
  } else if ((data[0] & 0xf8) == 0x88) {
    return add_instr2(ctx, ADC, 1, mk_arg_A, mk_GPR(data[0] & 0x7));
  } else if (((data[0] == 0xdd) || (data[0] == 0xfd)) && (data[1] == 0x8e)) {
    return add_instr2(ctx, ADC, 3, mk_arg_A, mk_arg(ARG_INDEX, data[0] & 0x20, data[2], true));
  } else if (data[0] == 0xce) {
    return add_instr2(ctx, ADC, 2, mk_arg_A, mk_arg(ARG_8IMM, data[1], 0, false));
  } else if ((data[0] == 0xed) && ((data[1] & 0xcf) == 0x4a)) {
    return add_instr2(ctx, ADC, 2, mk_arg_HL, mk_arg(ARG_REGPAIR_Q, (data[1] >> 4) & 0x3, 0, false));
  } else if (data[0] == 0x96) {
    return add_instr1(ctx, SUB, 1, mk_arg_HL_REF);
  } else if ((data[0] & 0xf8) == 0x90) {
    return add_instr1(ctx, SUB, 1, mk_GPR(data[0] & 0x7));
  } else if (((data[0] == 0xdd) || (data[0] == 0xfd)) && (data[1] == 0x96)) {
    return add_instr1(ctx, SUB, 3, mk_arg(ARG_INDEX, data[0] & 0x20, data[2], true));
  } else if (data[0] == 0xd6) {
    return add_instr1(ctx, SUB, 2, mk_arg(ARG_8IMM, data[1], 0, false));
  } else if (data[0] == 0x9e) {
    return add_instr2(ctx, SBC, 1, mk_arg_A, mk_arg_HL_REF);
  } else if ((data[0] & 0xf8) == 0x98) {
    return add_instr2(ctx, SBC, 1, mk_arg_A, mk_GPR(data[0] & 0x7));
  } else if (((data[0] == 0xdd) || (data[0] == 0xfd)) && (data[1] == 0x9e)) {
    return add_instr2(ctx, SBC, 3, mk_arg_A, mk_arg(ARG_INDEX, data[0] & 0x20, data[2], true));
  } else if (data[0] == 0xde) {
    return add_instr2(ctx, SBC, 2, mk_arg_A, mk_arg(ARG_8IMM, data[1], 0, false));
  } else if ((data[0] == 0xed) && ((data[1] & 0xcf) == 0x42)) {
    return add_instr2(ctx, SBC, 2, mk_arg_HL, mk_arg(ARG_REGPAIR_Q, (data[1] >> 4) & 0x3, 0, false));
  } else if (data[0] == 0xa6) {
    return add_instr1(ctx, AND, 1, mk_arg_HL_REF);
  } else if ((data[0] & 0xf8) == 0xa0) {
    return add_instr1(ctx, AND, 1, mk_GPR(data[0] & 0x7));
  } else if (((data[0] == 0xdd) || (data[0] == 0xfd)) && (data[1] == 0xa6)) {
    return add_instr1(ctx, AND, 3, mk_arg(ARG_INDEX, data[0] & 0x20, data[2], true));
  } else if (data[0] == 0xe6) {
    return add_instr1(ctx, AND, 2, mk_arg(ARG_8IMM, data[1], 0, false));
  } else if (data[0] == 0xae) {
    return add_instr1(ctx, XOR, 1, mk_arg_HL_REF);
  } else if ((data[0] & 0xf8) == 0xa8) {
    return add_instr1(ctx, XOR, 1, mk_GPR(data[0] & 0x7));
  } else if (((data[0] == 0xdd) || (data[0] == 0xfd)) && (data[1] == 0xae)) {
    return add_instr1(ctx, XOR, 3, mk_arg(ARG_INDEX, data[0] & 0x20, data[2], true));
  } else if (data[0] == 0xee) {
    return add_instr1(ctx, XOR, 2, mk_arg(ARG_8IMM, data[1], 0, false));
  } else if (data[0] == 0xb6) {
    return add_instr1(ctx, OR, 1, mk_arg_HL_REF);
  } else if ((data[0] & 0xf8) == 0xb0) {
    return add_instr1(ctx, OR, 1, mk_GPR(data[0] & 0x7));
  } else if (((data[0] == 0xdd) || (data[0] == 0xfd)) && (data[1] == 0xb6)) {
    return add_instr1(ctx, OR, 3, mk_arg(ARG_INDEX, data[0] & 0x20, data[2], true));
  } else if (data[0] == 0xf6) {
    return add_instr1(ctx, OR, 2, mk_arg(ARG_8IMM, data[1], 0, false));
  } else if (data[0] == 0xbe) {
    return add_instr1(ctx, CP, 1, mk_arg_HL_REF);
  } else if ((data[0] & 0xf8) == 0xb8) {
    return add_instr1(ctx, CP, 1, mk_GPR(data[0] & 0x7));
  } else if (((data[0] == 0xdd) || (data[0] == 0xfd)) && (data[1] == 0xbe)) {
    return add_instr1(ctx, CP, 3, mk_arg(ARG_INDEX, data[0] & 0x20, data[2], true));
  } else if (data[0] == 0xfe) {
    return add_instr1(ctx, CP, 2, mk_arg(ARG_8IMM, data[1], 0, false));
  } else if (data[0] == 0x34) {
    return add_instr1(ctx, INC, 1, mk_arg_HL_REF);
  } else if ((data[0] & 0xc7) == 0x04) {
    return add_instr1(ctx, INC, 1, mk_GPR((data[0] >> 3) & 0x7));
  } else if (((data[0] == 0xdd) || (data[0] == 0xfd)) && (data[1] == 0x34)) {
    return add_instr1(ctx, INC, 3, mk_arg(ARG_INDEX, data[0] & 0x20, data[2], true));
  } else if ((data[0] & 0xcf) == 0x03) {
    return add_instr1(ctx, INC, 1, mk_arg(ARG_REGPAIR_Q, (data[0] >> 4) & 0x3, 0, false));
  } else if (((data[0] == 0xdd) || (data[0] == 0xfd)) && (data[1] == 0x23)) {
    return add_instr1(ctx, INC, 2, mk_arg(ARG_REGPAIR_I, (data[0] & 0x20) ? REG_IY : REG_IX, 0, false));
  } else if (data[0] == 0x35) {
    return add_instr1(ctx, DEC, 1, mk_arg_HL_REF);
  } else if ((data[0] & 0xc7) == 0x05) {
    return add_instr1(ctx, DEC, 1, mk_GPR((data[0] >> 3) & 0x7));
  } else if (((data[0] == 0xdd) || (data[0] == 0xfd)) && (data[1] == 0x35)) {
    return add_instr1(ctx, DEC, 3, mk_arg(ARG_INDEX, data[0] & 0x20, data[2], true));
  } else if ((data[0] & 0xcf) == 0x0b) {
    return add_instr1(ctx, DEC, 1, mk_arg(ARG_REGPAIR_Q, (data[0] >> 4) & 0x3, 0, false));
  } else if (((data[0] == 0xdd) || (data[0] == 0xfd)) && (data[1] == 0x2b)) {
    return add_instr1(ctx, DEC, 2, mk_arg(ARG_REGPAIR_I, (data[0] & 0x20) ? REG_IY : REG_IX, 0, false));
  } else if (data[0] == 0x27) {
    return add_instr0(ctx, DAA, 1);
  } else if (data[0] == 0x2f) {
    return add_instr0(ctx, CPL, 1);
  } else if (data[0] == 0x3f) {
    return add_instr0(ctx, CCF, 1);
  } else if (data[0] == 0x37) {
    return add_instr0(ctx, SCF, 1);
  } else if (data[0] == 0xed && data[1] == 0x44) {
    return add_instr0(ctx, NEG, 2);
  } else if (data[0] == 0x27) {
    return add_instr0(ctx, DAA, 1);
  }

  return 0;
}

static int process_rotate_shift(disas_context_t *ctx, uint8_t *data) {
  if (data[0] == 0x07) {
    return add_instr0(ctx, RLCA, 1);
  } else if (data[0] == 0x0f) {
    return add_instr0(ctx, RRCA, 1);
  } else if (data[0] == 0x17) {
    return add_instr0(ctx, RLA, 1);
  } else if (data[0] == 0x1f) {
    return add_instr0(ctx, RRA, 1);
  } else if (data[0] == 0xed && data[1] == 0x6f) {
    return add_instr0(ctx, RLD, 2);
  } else if (data[0] == 0xed && data[1] == 0x67) {
    return add_instr0(ctx, RRD, 2);
  } else if (data[0] == 0xcb) {
    if (data[1] == 0x06) {
      return add_instr1(ctx, RLC, 2, mk_arg_HL_REF);
    } else if ((data[1] & 0xf8) == 0) {
      return add_instr1(ctx, RLC, 2, mk_GPR(data[1] & 0x7));
    } else if (data[1] == 0x0e) {
      return add_instr1(ctx, RRC, 2, mk_arg_HL_REF);
    } else if ((data[1] & 0xf8) == 0x08) {
      return add_instr1(ctx, RRC, 2, mk_GPR(data[1] & 0x7));
    } else if (data[1] == 0x16) {
      return add_instr1(ctx, RL, 2, mk_arg_HL_REF);
    } else if ((data[1] & 0xf8) == 0x10) {
      return add_instr1(ctx, RL, 2, mk_GPR(data[1] & 0x7));
    } else if (data[1] == 0x1e) {
      return add_instr1(ctx, RR, 2, mk_arg_HL_REF);
    } else if ((data[1] & 0xf8) == 0x18) {
      return add_instr1(ctx, RR, 2, mk_GPR(data[1] & 0x7));
    } else if (data[1] == 0x26) {
      return add_instr1(ctx, SLA, 2, mk_arg_HL_REF);
    } else if ((data[1] & 0xf8) == 0x20) {
      return add_instr1(ctx, SLA, 2, mk_GPR(data[1] & 0x7));
    } else if (data[1] == 0x2e) {
      return add_instr1(ctx, SRA, 2, mk_arg_HL_REF);
    } else if ((data[1] & 0xf8) == 0x28) {
      return add_instr1(ctx, SRA, 2, mk_GPR(data[1] & 0x7));
    } else if (data[1] == 0x3e) {
      return add_instr1(ctx, SRL, 2, mk_arg_HL_REF);
    } else if ((data[1] & 0xf8) == 0x38) {
      return add_instr1(ctx, SRL, 2, mk_GPR(data[1] & 0x7));
    }
  } else if (((data[0] == 0xdd) || (data[0] == 0xfd)) && (data[1] == 0xcb)) {
    if (data[3] == 0x06) {
      return add_instr1(ctx, RLC, 4, mk_arg(ARG_INDEX, data[0] & 0x20, data[2], true));
    } else if (data[3] == 0x0e) {
      return add_instr1(ctx, RRC, 4, mk_arg(ARG_INDEX, data[0] & 0x20, data[2], true));
    } else if (data[3] == 0x16) {
      return add_instr1(ctx, RL, 4, mk_arg(ARG_INDEX, data[0] & 0x20, data[2], true));
    } else if (data[3] == 0x1e) {
      return add_instr1(ctx, RR, 4, mk_arg(ARG_INDEX, data[0] & 0x20, data[2], true));
    } else if (data[3] == 0x26) {
      return add_instr1(ctx, SLA, 4, mk_arg(ARG_INDEX, data[0] & 0x20, data[2], true));
    } else if (data[3] == 0x2e) {
      return add_instr1(ctx, SRA,4,  mk_arg(ARG_INDEX, data[0] & 0x20, data[2], true));
    } else if (data[3] == 0x3e) {
      return add_instr1(ctx, SRL, 4, mk_arg(ARG_INDEX, data[0] & 0x20, data[2], true));
    }
  }

  return 0;
}

static int process_bit_manipulation(disas_context_t *ctx, uint8_t *data) {
  if (data[0] == 0xcb) {
    if ((data[1] & 0xc7) == 0x46) {
      return add_instr2(ctx, BIT, 2, mk_arg(ARG_BITNUM, (data[1] >> 3) & 0x7, 0, false), mk_arg_HL_REF);
    } else if ((data[1] & 0xc0) == 0x40) {
      return add_instr2(ctx, BIT, 2, mk_arg(ARG_BITNUM, (data[1] >> 3) & 0x7, 0, false), mk_GPR(data[1] & 0x7));
    } else if ((data[1] & 0xc7) == 0x86) {
      return add_instr2(ctx, RES, 2, mk_arg(ARG_BITNUM, (data[1] >> 3) & 0x7, 0, false), mk_arg_HL_REF);
    } else if ((data[1] & 0xc0) == 0x80) {
      return add_instr2(ctx, RES, 2, mk_arg(ARG_BITNUM, (data[1] >> 3) & 0x7, 0, false), mk_GPR(data[1] & 0x7));
    } else if ((data[1] & 0xc7) == 0xc6) {
      return add_instr2(ctx, SET, 2, mk_arg(ARG_BITNUM, (data[1] >> 3) & 0x7, 0, false), mk_arg_HL_REF);
    } else if ((data[1] & 0xc0) == 0xc0) {
      return add_instr2(ctx, SET, 2, mk_arg(ARG_BITNUM, (data[1] >> 3) & 0x7, 0, false), mk_GPR(data[1] & 0x7));
    }
  } else if (((data[0] == 0xdd) || (data[0] == 0xfd)) && (data[1] == 0xcb)) {
    if ((data[3] & 0xc7) == 0x86) {
      return add_instr2(ctx, RES, 4, mk_arg(ARG_BITNUM, (data[3] >> 3) & 0x7, 0, false), mk_arg(ARG_INDEX, data[0] & 0x20, data[2], true));
    } else if ((data[3] & 0xc7) == 0xc6) {
      return add_instr2(ctx, SET, 4, mk_arg(ARG_BITNUM, (data[3] >> 3) & 0x7, 0, false), mk_arg(ARG_INDEX, data[0] & 0x20, data[2], true));
    } else if ((data[3] & 0xc0) == 0x40) {
      return add_instr2(ctx, BIT, 4, mk_arg(ARG_BITNUM, (data[3] >> 3) & 0x7, 0, false), mk_arg(ARG_INDEX, data[0] & 0x20, data[2], true));
    }
  }

  return 0;
}

static int process_jump_call_ret(disas_context_t *ctx, uint8_t *data) {
  if (data[0] == 0xc3) {
    ADD_LABEL(ctx, (uint16_t)data[1] | ((uint16_t)data[2] << 8));
    return add_instr1(ctx, JP, 3, mk_arg(ARG_16IMM, data[1], data[2], false));
  } else if (data[0] == 0xe9) {
    return add_instr1(ctx, JP, 1, mk_arg_HL_REF);
  } else if (((data[0] == 0xdd) || (data[0] == 0xfd)) && (data[1] == 0xe9)) {
    return add_instr1(ctx, JP, 2, mk_arg(ARG_REGPAIR_I, (data[0] & 0x20) ? REG_IY : REG_IX, 0, true));
  } else if ((data[0] & 0xc7) == 0xc2) {
    ADD_LABEL(ctx, (uint16_t)data[1] | ((uint16_t)data[2] << 8));
    return add_instr2(ctx, JP, 3, mk_arg(ARG_CONDITION, (data[0] >> 3) & 0x7, 0, false), mk_arg(ARG_16IMM, data[1], data[2], false));
  } else if (data[0] == 0x18) {
    return add_instr1(ctx, JR, 2, mk_arg(ARG_RELJUMP, data[1], 2, false));
  } else if (data[0] == 0x20) {
    return add_instr2(ctx, JR, 2, mk_arg(ARG_CONDITION, COND_NZ, 0, false), mk_arg(ARG_RELJUMP, data[1], 2, false));
  } else if (data[0] == 0x28) {
    return add_instr2(ctx, JR, 2, mk_arg(ARG_CONDITION, COND_Z, 0, false), mk_arg(ARG_RELJUMP, data[1], 2, false));
  } else if (data[0] == 0x30) {
    return add_instr2(ctx, JR, 2, mk_arg(ARG_CONDITION, COND_NC, 0, false), mk_arg(ARG_RELJUMP, data[1], 2, false));
  } else if (data[0] == 0x38) {
    return add_instr2(ctx, JR, 2, mk_arg(ARG_CONDITION, COND_C, 0, false), mk_arg(ARG_RELJUMP, data[1], 2, false));
  } else if (data[0] == 0xcd) {
    ADD_LABEL(ctx, (uint16_t)data[1] | ((uint16_t)data[2] << 8));
    return add_instr1(ctx, CALL, 3, mk_arg(ARG_16IMM, data[1], data[2], false));
  } else if ((data[0] & 0xc7) == 0xc4) {
    ADD_LABEL(ctx, (uint16_t)data[1] | ((uint16_t)data[2] << 8));
    return add_instr2(ctx, CALL, 3, mk_arg(ARG_CONDITION, (data[0] >> 3) & 0x7, 0, false), mk_arg(ARG_16IMM, data[1], data[2], false));
  } else if (data[0] == 0x10) {
    return add_instr1(ctx, DJNZ, 2, mk_arg(ARG_RELJUMP, data[1], 2, false));
  } else if (data[0] == 0xc9) {
    return add_instr0(ctx, RET, 1);
  } else if ((data[0] & 0xc7) == 0xc0) {
    return add_instr1(ctx, RET, 1, mk_arg(ARG_CONDITION, (data[0] >> 3) & 0x7, 0, false));
  } else if (data[0] == 0xed && data[1] == 0x4d) {
    return add_instr0(ctx, RETI, 2);
  } else if (data[0] == 0xed && data[1] == 0x45) {
    return add_instr0(ctx, RETN, 2);
  } else if ((data[0] & 0xc7) == 0xc7) {
    return add_instr1(ctx, RST, 1, mk_arg(ARG_RST, (data[0] >> 3) & 0x7, 0, false));
  }

  return 0;
}

static int process_io(disas_context_t *ctx, uint8_t *data) {
  if (data[0] == 0xdb) {
    return add_instr2(ctx, IN, 2, mk_arg_A, mk_arg(ARG_8IMM, data[1], 0, true));
  } else if ((data[0] == 0xed) && ((data[1] & 0xc7) == 0x40)) {
    return add_instr2(ctx, IN, 2, mk_GPR((data[1] >> 3) & 0x7), mk_arg(ARG_8GPR, REG_C, 0, true));
  } else if ((data[0] == 0xed) && (data[1] == 0xaa)) {
    return add_instr0(ctx, IND, 2);
  } else if ((data[0] == 0xed) && (data[1] == 0xba)) {
    return add_instr0(ctx, INDR, 2);
  } else if ((data[0] == 0xed) && (data[1] == 0xa2)) {
    return add_instr0(ctx, INI, 2);
  } else if ((data[0] == 0xed) && (data[1] == 0xb2)) {
    return add_instr0(ctx, INIR, 2);
  } else if (data[0] == 0xd3) {
    return add_instr2(ctx, OUT, 2, mk_arg_A, mk_arg(ARG_8IMM, data[1], 0, true));
  } else if ((data[0] == 0xed) && ((data[1] & 0xc7) == 0x41)) {
    return add_instr2(ctx, OUT, 2, mk_GPR((data[1] >> 3) & 0x7), mk_arg(ARG_8GPR, REG_C, 0, true));
  } else if ((data[0] == 0xed) && (data[1] == 0xab)) {
    return add_instr0(ctx, OUTD, 2);
  } else if ((data[0] == 0xed) && (data[1] == 0xbb)) {
    return add_instr0(ctx, OTDR, 2);
  } else if ((data[0] == 0xed) && (data[1] == 0xa3)) {
    return add_instr0(ctx, OUTI, 2);
  } else if ((data[0] == 0xed) && (data[1] == 0xb3)) {
    return add_instr0(ctx, OTIR, 2);
  }

  return 0;
}

static int process_cpucontrol(disas_context_t *ctx, uint8_t *data) {
  if (data[0] == 0x0) {
    return add_instr0(ctx, NOP, 1);
  } else if (data[0] == 0x76) {
    return add_instr0(ctx, HALT, 1);
  } else if (data[0] == 0xf3) {
    return add_instr0(ctx, DI, 1);
  } else if (data[0] == 0xfb) {
    return add_instr0(ctx, EI, 1);
  } else if (data[0] == 0xed) {
    if (data[1] == 0x46) {
      return add_instr1(ctx, IM, 2, mk_arg(ARG_8IMM, 0, 0, false));
    } else if (data[1] == 0x56) {
      return add_instr1(ctx, IM, 2, mk_arg(ARG_8IMM, 1, 0, false));
    } else if (data[1] == 0x5e) {
      return add_instr1(ctx, IM, 2, mk_arg(ARG_8IMM, 2, 0, false));
    }
  }

  return 0;
}

char *disassemble(char *data,
                  ssize_t size,
                  char *filename,
                  bool opt_addr,
                  bool opt_source,
                  bool opt_labels,
                  int opt_org) {
  disas_context_t context;
  disas_node_t *node;
  uint8_t *p = (uint8_t *)data;

  memset(&context, 0, sizeof(context));

  context.opt_addr = opt_addr;
  context.opt_labels = opt_labels;
  context.opt_source = opt_source;
  context.in_filename = filename;
  context.org = context.curr_addr = opt_org;

  context.nodes = context.last_node = NULL;
  context.binary = (uint8_t *)data;
  memset(&context.labels_bmp, 0, sizeof(context.labels_bmp));

  while (size > 0) {
    int isize = 0;

    if (isize == 0)
      isize = process_ld(&context, p);
    if (isize == 0)
      isize = process_pushpop(&context, p);
    if (isize == 0)
      isize = process_ex_exx(&context, p);
    if (isize == 0)
      isize = process_block_transfer_search(&context, p);
    if (isize == 0)
      isize = process_arithmetic(&context, p);
    if (isize == 0)
      isize = process_rotate_shift(&context, p);
    if (isize == 0)
      isize = process_bit_manipulation(&context, p);
    if (isize == 0)
      isize = process_jump_call_ret(&context, p);
    if (isize == 0)
      isize = process_io(&context, p);
    if (isize == 0)
      isize = process_cpucontrol(&context, p);

    if (isize == 0) {
      int inv_isize = add_invalid_instr(&context, p[0]);
      p += inv_isize;
      size -= inv_isize;
    } else {
      p += isize;
      size -= isize;
    }
  }

  context.out_capacity = 1024;
  context.out_str = malloc(context.out_capacity);
  context.out_len = 0;

  disas_render_text(&context);

  for (node = context.nodes; node != NULL; node = node->next)
    free(node);

  return context.out_str;
}


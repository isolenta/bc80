#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>

#include "asm/parse.h"
#include "asm/render.h"
#include "common/dynarray.h"

#include "instr.inc.c"

static const char *reserved_ids[] = {
  "A", "B", "C", "D", "E", "H", "L", "F", "I", "R",
  "BC", "DE", "HL", "AF", "AF'", "SP",
  "IX", "IY", "IXH", "IXL", "IYH", "IYL",
  "NZ", "Z", "NC", "C", "PO", "PE", "P", "M", NULL};

static bool contains_reserved_ids(parse_node *node) {
  if (node == NULL)
    return false;

  if (node->type == NODE_ID) {
    ID *id = (ID *)node;

    for (int i = 0;; i++) {
      const char *rsvname = reserved_ids[i];
      if (rsvname == NULL)
        break;

      if (strcasecmp(id->name, rsvname) == 0)
        return true;
    }
  } else if (node->type == NODE_EXPR) {
    EXPR *expr = (EXPR *)node;

    return contains_reserved_ids(expr->left) || contains_reserved_ids(expr->right);
  }

  return false;
}

static inline void check_integer_overflow(compile_ctx_t *ctx, int value, int nbytes)
{
  const char *msg;
  unsigned mask;

  assert(nbytes == 1 || nbytes == 2);

  if (nbytes == 1) {
    msg = "byte";
    mask = 0xff;
  } else if (nbytes == 2) {
    msg = "word";
    mask = 0xffff;
  }

  if (value & ~mask)
    report_warning(ctx, "%s value %d (0x%x) truncated",
      msg, value, value);
}

static inline void check_reljump_overflow(compile_ctx_t *ctx, int value, char *instr_name)
{
  if (value & ~0xff)
    report_error(ctx, "%s offset %d doesn't fit in byte", instr_name, value);
}

static inline void check_bitnum(compile_ctx_t *ctx, int value, char *instr_name)
{
  if (value < 0 || value > 7)
    report_error(ctx, "invalid bit number %d in %s", value, instr_name);
}

static inline bool check_id_name(ID *id, char *name) {
  return strcasecmp(id->name, name) == 0;
}

static inline void check_arg_presence(compile_ctx_t *ctx, parse_node *argnode, int argid)
{
  if (argnode == NULL)
    report_error(ctx, "argument %d missing", argid);
}

#define CHECK_VALID_NODE(_n_)  \
  if ((_n_) == NULL)          \
    return false

#define CHECK_ARG_IS_ID(_n_)  \
  if ((_n_)->type != NODE_ID) \
    return false;             \
  ID *id = (ID *)(_n_)

static bool try_get_arg_accum(parse_node *node) {
  CHECK_VALID_NODE(node);
  CHECK_ARG_IS_ID(node);
  return check_id_name(id, "a");
}

static bool try_get_arg_flags(parse_node *node) {
  CHECK_VALID_NODE(node);
  CHECK_ARG_IS_ID(node);
  return check_id_name(id, "f");
}

static bool try_get_arg_intreg(parse_node *node) {
  CHECK_VALID_NODE(node);
  CHECK_ARG_IS_ID(node);
  return check_id_name(id, "i");
}

static bool try_get_arg_rfshreg(parse_node *node) {
  CHECK_VALID_NODE(node);
  CHECK_ARG_IS_ID(node);
  return check_id_name(id, "r");
}

static bool try_get_arg_gpr8(parse_node *node, int *reg_opcode, bool *is_ref) {
  CHECK_VALID_NODE(node);
  CHECK_ARG_IS_ID(node);

  if (check_id_name(id, "a"))
    *reg_opcode = REG_A;
  else if (check_id_name(id, "b"))
    *reg_opcode = REG_B;
  else if (check_id_name(id, "c"))
    *reg_opcode = REG_C;
  else if (check_id_name(id, "d"))
    *reg_opcode = REG_D;
  else if (check_id_name(id, "e"))
    *reg_opcode = REG_E;
  else if (check_id_name(id, "h"))
    *reg_opcode = REG_H;
  else if (check_id_name(id, "l"))
    *reg_opcode = REG_L;
  else
    return false;

  *is_ref = id->hdr.is_ref;

  return true;
}

static bool try_get_arg_hl(parse_node *node, bool *is_ref) {
  CHECK_VALID_NODE(node);
  CHECK_ARG_IS_ID(node);

  if (check_id_name(id, "hl")) {
    *is_ref = id->hdr.is_ref;
    return true;
  }

  return false;
}

static bool try_get_arg_qreg16(parse_node *node, int *reg_opcode, bool *is_ref) {
  CHECK_VALID_NODE(node);
  CHECK_ARG_IS_ID(node);

  if (check_id_name(id, "bc"))
    *reg_opcode = 0x0;
  else if (check_id_name(id, "de"))
    *reg_opcode = 0x1;
  else if (check_id_name(id, "hl"))
    *reg_opcode = 0x2;
  else if (check_id_name(id, "sp"))
    *reg_opcode = 0x3;
  else
    return false;

  *is_ref = id->hdr.is_ref;

  return true;
}

static bool try_get_arg_preg16(parse_node *node, int *reg_opcode, bool *is_ref) {
  CHECK_VALID_NODE(node);
  CHECK_ARG_IS_ID(node);

  if (check_id_name(id, "bc"))
    *reg_opcode = 0x0;
  else if (check_id_name(id, "de"))
    *reg_opcode = 0x1;
  else if (check_id_name(id, "hl"))
    *reg_opcode = 0x2;
  else if (check_id_name(id, "af"))
    *reg_opcode = 0x3;
  else
    return false;

  *is_ref = id->hdr.is_ref;

  return true;
}

static bool try_get_arg_af(parse_node *node) {
  CHECK_VALID_NODE(node);
  CHECK_ARG_IS_ID(node);
  return check_id_name(id, "af");
}

static bool try_get_arg_af_shadow(parse_node *node) {
  CHECK_VALID_NODE(node);
  CHECK_ARG_IS_ID(node);
  return check_id_name(id, "af'");
}

static bool try_get_arg_index(parse_node *node, int *i, bool *is_ref) {
  CHECK_VALID_NODE(node);
  CHECK_ARG_IS_ID(node);

  if (check_id_name(id, "ix"))
    *i = REG_IX;
  else if (check_id_name(id, "iy"))
    *i = REG_IY;
  else
    return false;

  *is_ref = id->hdr.is_ref;

  return true;
}

static bool try_get_arg_ixy_half(parse_node *node, int *prefix, int *i) {
  CHECK_VALID_NODE(node);
  CHECK_ARG_IS_ID(node);

  if (id->hdr.is_ref)
    return false;

  if (check_id_name(id, "ixl")) {
    *i = REG_IXL;
    *prefix = 0xdd;
  } else if (check_id_name(id, "ixh")) {
    *i = REG_IXH;
    *prefix = 0xdd;
  } else if (check_id_name(id, "iyl")) {
    *i = REG_IYL;
    *prefix = 0xfd;
  } else if (check_id_name(id, "iyh")) {
    *i = REG_IYH;
    *prefix = 0xfd;
  } else
    return false;

  return true;
}

static bool try_get_arg_8imm(compile_ctx_t *ctx, parse_node *node, int *ival, bool *is_ref, int imm_pos) {
  CHECK_VALID_NODE(node);
  if (node->type == NODE_LITERAL) {
    LITERAL *l = (LITERAL *)node;
    if (l->kind == INT) {
      *ival = l->ival;
      *is_ref = l->hdr.is_ref;
      return true;
    }
  } else if ((imm_pos != -1) && !contains_reserved_ids(node)) {
    // will patch operand position with literal value at 2nd pass

    *is_ref = node->is_ref;
    *ival = 0;
    register_fwd_lookup(ctx, node, imm_pos, 1, false, 0);

    return true;
  }
  return false;
}

static bool try_get_arg_16imm(compile_ctx_t *ctx,
                          parse_node *node,
                          int *lsb, int *msb,
                          int *orig_val,
                          bool *is_ref,
                          int imm_pos) {
  CHECK_VALID_NODE(node);
  *is_ref = node->is_ref;
  if (node->type == NODE_LITERAL) {
    LITERAL *l = (LITERAL *)node;
    if (l->kind == INT) {
      *orig_val = l->ival;
      *lsb = l->ival & 0xFF;
      *msb = (l->ival >> 8) & 0xFF;
      return true;
    }
  } else if ((imm_pos != -1) && !contains_reserved_ids(node)) {
    // will patch operand position with literal value at 2nd pass

    *lsb = 0;
    *msb = 0;
    *orig_val = 0;
    register_fwd_lookup(ctx, node, imm_pos, 2, false, 0);

    return true;
  }
  return false;
}

static bool try_get_arg_index_offset8(compile_ctx_t *ctx, parse_node *node, int *idx, int *offset, int imm_pos) {
  CHECK_VALID_NODE(node);
  if (node->type != NODE_EXPR)
    return false;

  if (!node->is_ref)
    return false;

  EXPR *expr = (EXPR *)node;
  if (expr->kind != BINARY_PLUS && expr->kind != BINARY_MINUS)
    return false;

  if (expr->left->type != NODE_ID)
    return false;

  ID *idx_arg = (ID *)expr->left;
  if (check_id_name(idx_arg, "ix"))
    *idx = REG_IX;
  else if (check_id_name(idx_arg, "iy"))
    *idx = REG_IY;
  else
    return false;

  if (expr->right->type == NODE_LITERAL) {
    LITERAL *offs_lit = (LITERAL *)expr->right;

    if (offs_lit->kind != INT)
      return false;

    *offset = offs_lit->ival;
    if (expr->kind == BINARY_MINUS)
      *offset = -(*offset);

    return true;
  } else if ((imm_pos != -1) && !contains_reserved_ids(node)) {
    // will patch operand position with literal value at 2nd pass

    *offset = 0;
    register_fwd_lookup(ctx, node, imm_pos, 1, false, 0);

    return true;
  }

  return false;
}

static bool try_get_arg_bitnum(parse_node *node, int *bitnum) {
  bool is_ref;

  CHECK_VALID_NODE(node);

  // try_get_arg_8imm() doesn't need context for non-forward bitnum resolution so it's safe to pass NULL here
  bool result = try_get_arg_8imm(NULL, node, bitnum, &is_ref, -1);
  if ((*bitnum >= 0) && (*bitnum <= 7) && !is_ref)
    return result;
  return false;
}

static bool try_get_arg_condition(parse_node *node, int *cond) {
  CHECK_VALID_NODE(node);
  CHECK_ARG_IS_ID(node);

  if (check_id_name(id, "nz"))
    *cond = COND_NZ;
  else if (check_id_name(id, "z"))
    *cond = COND_Z;
  else if (check_id_name(id, "nc"))
    *cond = COND_NC;
  else if (check_id_name(id, "c"))
    *cond = COND_C;
  else if (check_id_name(id, "po"))
    *cond = COND_PO;
  else if (check_id_name(id, "pe"))
    *cond = COND_PE;
  else if (check_id_name(id, "p"))
    *cond = COND_P;
  else if (check_id_name(id, "m"))
    *cond = COND_M;
  else
    return false;

  return true;
}

static bool try_get_arg_reladdr(compile_ctx_t *ctx, parse_node *node, int *reladdr, int imm_pos) {
  CHECK_VALID_NODE(node);

// any relative jump instructions have 2 bytes length (JR, DJNZ)
#define JUMP_REL_INSTR_SIZE 2

  int lsb, msb;
  section_ctx_t *section = get_current_section(ctx);

  if (node->type == NODE_LITERAL) {
    LITERAL *l = (LITERAL *)node;
    if (node->is_ref)
      return false;

    if (l->kind == INT) {
      int addr = l->ival;
      *reladdr = addr - section->curr_pc - JUMP_REL_INSTR_SIZE;
      if ((*reladdr >= -128) && (*reladdr <= 127))
        return true;
    }
  } else if ((imm_pos != -1) && !contains_reserved_ids(node)) {
    // will patch operand position with literal value at 2nd pass
    *reladdr = 0;
    register_fwd_lookup(ctx, node, imm_pos, 1, true, section->curr_pc + JUMP_REL_INSTR_SIZE);

    return true;
  }
  return false;
}

static bool try_get_arg_rstaddr(parse_node *node, int *rstcode, int *orig_val) {
  bool is_ref;

  CHECK_VALID_NODE(node);

  // try_get_arg_8imm() doesn't need context for non-forward rstcode resolution so it's safe to pass NULL here
  if (try_get_arg_8imm(NULL, node, orig_val, &is_ref, -1) && !is_ref) {
    switch (*orig_val) {
      case 0x00:
        *rstcode = 0;
        return true;
      case 0x08:
        *rstcode = 1;
        return true;
      case 0x10:
        *rstcode = 2;
        return true;
      case 0x18:
        *rstcode = 3;
        return true;
      case 0x20:
        *rstcode = 4;
        return true;
      case 0x28:
        *rstcode = 5;
        return true;
      case 0x30:
        *rstcode = 6;
        return true;
      case 0x38:
        *rstcode = 7;
        return true;
      default:
        return false;
    }
  }

  return false;
}

void compile_instruction_impl(compile_ctx_t *ctx, char *name, dynarray *args) {
  section_ctx_t *section = get_current_section(ctx);

  #define ERR_UNEXPECTED_ARGUMENT(n) do { \
    report_error(ctx, "unexpected argument %d", (n)); \
  } while(0)

  // get instruction ordinal id
  MnemonicEnum i_mnemonic = MAX_MNEMONIC_ID;
  for (int i = 0; i < MAX_MNEMONIC_ID; i++) {
    if (strcasecmp(name, MnemonicStrings[i]) == 0)
      i_mnemonic = (MnemonicEnum)i;
  }

  if (i_mnemonic == MAX_MNEMONIC_ID) {
    report_error(ctx, "no such instruction %s", name);
    return;
  }

  // check number of arguments
  int num_args = dynarray_length(args);
  int chk_mask = 0;

  if (num_args == 0)
    chk_mask = NARGS_0;
  else if (num_args == 1)
    chk_mask = NARGS_1;
  else if (num_args == 2)
    chk_mask = NARGS_2;

  if (!(NUM_ARGS[(int)i_mnemonic] & chk_mask))
    report_error(ctx, "invalid number of arguments %d", num_args);

  // evaluate arguments expressions
  parse_node *arg1 = NULL;
  parse_node *arg2 = NULL;

  if (dynarray_length(args) > 0)
    arg1 = dinitial(args);

  if (dynarray_length(args) > 1)
    arg2 = dsecond(args);

  bool is_ref;
  int opc, opc2;

  // compile it
  switch (i_mnemonic) {
    case ADC: {
      check_arg_presence(ctx, arg1, 1);
      check_arg_presence(ctx, arg2, 2);

      if (try_get_arg_accum(arg1)) {
        if (try_get_arg_gpr8(arg2, &opc, &is_ref) && !is_ref)
          render_byte(ctx, 0x88 | opc, 4);
        else if (try_get_arg_8imm(ctx, arg2, &opc, &is_ref, section->curr_pc + 2) && !is_ref) {
          check_integer_overflow(ctx, opc, 1);
          render_2bytes(ctx, 0xCE, opc, 7);
        } else if (try_get_arg_hl(arg2, &is_ref) && is_ref)
          render_byte(ctx, 0x8E, 7);
        else if (try_get_arg_index_offset8(ctx, arg2, &opc, &opc2, section->curr_pc + 2)) {
          check_integer_overflow(ctx, opc2, 1);
          render_3bytes(ctx, 0xDD | (opc << 5), 0x8E, opc2, 19);
        } else if (try_get_arg_ixy_half(arg2, &opc, &opc2))
          render_2bytes(ctx, opc, 0x88 | opc2, 8);
        else
          ERR_UNEXPECTED_ARGUMENT(2);
      } else if (try_get_arg_hl(arg1, &is_ref) && !is_ref) {
        if (try_get_arg_qreg16(arg2, &opc, &is_ref) && !is_ref)
          render_2bytes(ctx, 0xED, 0x4A | (opc << 4), 15);
        else
          ERR_UNEXPECTED_ARGUMENT(2);
      } else {
        ERR_UNEXPECTED_ARGUMENT(1);
      }

      break;
    }

    case ADD: {
      check_arg_presence(ctx, arg1, 1);
      check_arg_presence(ctx, arg2, 2);

      if (try_get_arg_accum(arg1)) {
        if (try_get_arg_gpr8(arg2, &opc, &is_ref) && !is_ref)
          render_byte(ctx, 0x80 | opc, 4);
        else if (try_get_arg_8imm(ctx, arg2, &opc, &is_ref, section->curr_pc + 2) && !is_ref) {
          check_integer_overflow(ctx, opc, 1);
          render_2bytes(ctx, 0xC6, opc, 7);
        } else if (try_get_arg_hl(arg2, &is_ref) && is_ref)
          render_byte(ctx, 0x86, 7);
        else if (try_get_arg_index_offset8(ctx, arg2, &opc, &opc2, section->curr_pc + 2)) {
          check_integer_overflow(ctx, opc2, 1);
          render_3bytes(ctx, 0xDD | (opc << 5), 0x86, opc2, 19);
        } else if (try_get_arg_ixy_half(arg2, &opc, &opc2))
          render_2bytes(ctx, opc, 0x80 | opc2, 8);
        else
          ERR_UNEXPECTED_ARGUMENT(2);
      } else if (try_get_arg_hl(arg1, &is_ref) && !is_ref) {
        if (try_get_arg_qreg16(arg2, &opc, &is_ref) && !is_ref)
          render_byte(ctx, 0x09 | (opc << 4), 11);
        else
          ERR_UNEXPECTED_ARGUMENT(2);
      } else if (try_get_arg_index(arg1, &opc, &is_ref) && !is_ref) {
        if (try_get_arg_qreg16(arg2, &opc2, &is_ref) && !is_ref)
          render_2bytes(ctx, 0xDD | (opc << 5), 0x09 | (opc2 << 4), 15);
        else
          ERR_UNEXPECTED_ARGUMENT(2);
      } else {
        ERR_UNEXPECTED_ARGUMENT(1);
      }

      break;
    }

    case AND: {
      check_arg_presence(ctx, arg1, 1);

      if (try_get_arg_gpr8(arg1, &opc, &is_ref) && !is_ref)
        render_byte(ctx, 0xA0 | opc, 4);
      else if (try_get_arg_8imm(ctx, arg1, &opc, &is_ref, section->curr_pc + 2) && !is_ref) {
        check_integer_overflow(ctx, opc, 1);
        render_2bytes(ctx, 0xE6, opc, 7);
      } else if (try_get_arg_hl(arg1, &is_ref) && is_ref)
        render_byte(ctx, 0xA6, 7);
      else if (try_get_arg_index_offset8(ctx, arg1, &opc, &opc2, section->curr_pc + 2))
        render_3bytes(ctx, 0xDD | (opc << 5), 0xA6, opc2, 19);
      else if (try_get_arg_ixy_half(arg1, &opc, &opc2))
        render_2bytes(ctx, opc, 0xA0 | opc2, 8);
      else
        ERR_UNEXPECTED_ARGUMENT(1);

      break;
    }

    case BIT: {
      int b;

      check_arg_presence(ctx, arg1, 1);
      check_arg_presence(ctx, arg2, 2);

      if (try_get_arg_bitnum(arg1, &b)) {
        bool is_ref;
        if (try_get_arg_gpr8(arg2, &opc, &is_ref) && !is_ref)
          render_2bytes(ctx, 0xCB, 0x40 | (b << 3) | opc, 8);
        else if (try_get_arg_hl(arg2, &is_ref) && is_ref)
          render_2bytes(ctx, 0xCB, 0x46 | (b << 3), 12);
        else if (try_get_arg_index_offset8(ctx, arg2, &opc, &opc2, section->curr_pc + 2))
          render_4bytes(ctx, 0xDD | (opc << 5), 0xCB, opc2, 0x46 | (b << 3), 20);
        else
          ERR_UNEXPECTED_ARGUMENT(2);
      } else {
        check_bitnum(ctx, b, "BIT");
        ERR_UNEXPECTED_ARGUMENT(1);
      }

      break;
    }

    case CALL: {
      int val;

      check_arg_presence(ctx, arg1, 1);

      if (num_args == 1) {
        int lsb, msb;

        if (try_get_arg_16imm(ctx, arg1, &lsb, &msb, &val, &is_ref, section->curr_pc - section->start + 1) && !is_ref) {
          check_integer_overflow(ctx, val, 2);
          render_3bytes(ctx, 0xCD, lsb, msb, 17);
        } else
          ERR_UNEXPECTED_ARGUMENT(1);
      } else if (num_args == 2) {
        int cond, lsb, msb;

        check_arg_presence(ctx, arg2, 2);

        if (try_get_arg_condition(arg1, &cond)) {
          if (try_get_arg_16imm(ctx, arg2, &lsb, &msb, &val, &is_ref, section->curr_pc - section->start + 1) && !is_ref) {
            check_integer_overflow(ctx, val, 2);
            render_3bytes(ctx, 0xC4 | (cond << 3), lsb, msb, 17);
          } else
            ERR_UNEXPECTED_ARGUMENT(2);
        } else {
          ERR_UNEXPECTED_ARGUMENT(1);
        }
      }

      break;
    }

    case CCF: {
      render_byte(ctx, 0x3F, 4);
      break;
    }

    case CP: {
      check_arg_presence(ctx, arg1, 1);

      if (try_get_arg_gpr8(arg1, &opc, &is_ref) && !is_ref)
        render_byte(ctx, 0xB8 | opc, 4);
      else if (try_get_arg_8imm(ctx, arg1, &opc, &is_ref, section->curr_pc - section->start + 2) && !is_ref) {
        check_integer_overflow(ctx, opc, 1);
        render_2bytes(ctx, 0xFE, opc, 7);
      } else if (try_get_arg_hl(arg1, &is_ref) && is_ref)
        render_byte(ctx, 0xBE, 7);
      else if (try_get_arg_index_offset8(ctx, arg1, &opc, &opc2, section->curr_pc - section->start + 2))
        render_3bytes(ctx, 0xDD | (opc << 5), 0xBE, opc2, 19);
      else if (try_get_arg_ixy_half(arg1, &opc, &opc2))
        render_2bytes(ctx, opc, 0xB8 | opc2, 8);
      else
        ERR_UNEXPECTED_ARGUMENT(1);

      break;
    }

    case CPD: {
      render_2bytes(ctx, 0xED, 0xA9, 16);
      break;
    }

    case CPDR: {
      render_2bytes(ctx, 0xED, 0xB9, 21);
      break;
    }

    case CPI: {
      render_2bytes(ctx, 0xED, 0xA1, 16);
      break;
    }

    case CPIR: {
      render_2bytes(ctx, 0xED, 0xB1, 21);
      break;
    }

    case CPL: {
      render_byte(ctx, 0x2F, 4);
      break;
    }

    case DAA: {
      render_byte(ctx, 0x27, 4);
      break;
    }

    case DEC: {
      check_arg_presence(ctx, arg1, 1);

      if (try_get_arg_gpr8(arg1, &opc, &is_ref) && !is_ref)
        render_byte(ctx, 0x05 | (opc << 3), 4);
      else if (try_get_arg_hl(arg1, &is_ref) && is_ref)
        render_byte(ctx, 0x35, 11);
      else if (try_get_arg_index_offset8(ctx, arg1, &opc, &opc2, section->curr_pc + 2))
        render_3bytes(ctx, 0xDD | (opc << 5), 0x35, opc2, 23);
      else if (try_get_arg_qreg16(arg1, &opc2, &is_ref) && !is_ref)
        render_byte(ctx, 0x0B | (opc2 << 4), 6);
      else if (try_get_arg_index(arg1, &opc, &is_ref) && !is_ref)
        render_2bytes(ctx, 0xDD | (opc << 5), 0x2B, 10);
      else if (try_get_arg_ixy_half(arg1, &opc, &opc2))
        render_2bytes(ctx, opc, 0x05 | (opc2 << 3), 8);
      else
        ERR_UNEXPECTED_ARGUMENT(1);

      break;
    }

    case DI: {
      render_byte(ctx, 0xF3, 4);
      break;
    }

    case DJNZ: {
      int reladdr;

      check_arg_presence(ctx, arg1, 1);

      if (try_get_arg_reladdr(ctx, arg1, &reladdr, section->curr_pc - section->start + 1))
        render_2bytes(ctx, 0x10, reladdr, 13);
      else {
        check_reljump_overflow(ctx, reladdr, "DJNZ");
        ERR_UNEXPECTED_ARGUMENT(1);
      }

      break;
    }

    case EI: {
      render_byte(ctx, 0xFB, 4);
      break;
    }

    case EX: {
      check_arg_presence(ctx, arg1, 1);
      check_arg_presence(ctx, arg2, 2);

      if (try_get_arg_qreg16(arg1, &opc, &is_ref) && (opc == REG_SP) && is_ref) {
        if (try_get_arg_hl(arg2, &is_ref) && !is_ref)
          render_byte(ctx, 0xE3, 19);
        else if (try_get_arg_index(arg2, &opc, &is_ref) && !is_ref)
          render_2bytes(ctx, 0xDD | (opc << 5), 0xE3, 23);
        else
          ERR_UNEXPECTED_ARGUMENT(2);
      } else if (try_get_arg_af(arg1)) {
        if (try_get_arg_af_shadow(arg2))
          render_byte(ctx, 0x08, 4);
        else
          ERR_UNEXPECTED_ARGUMENT(2);
      } else if (try_get_arg_qreg16(arg1, &opc, &is_ref) && (opc == REG_DE) && !is_ref) {
        if (try_get_arg_hl(arg2, &is_ref) && !is_ref)
          render_byte(ctx, 0xEB, 4);
        else
          ERR_UNEXPECTED_ARGUMENT(2);
      } else {
        ERR_UNEXPECTED_ARGUMENT(1);
      }

      break;
    }

    case EXX: {
      render_byte(ctx, 0xD9, 4);
      break;
    }

    case HALT: {
      render_byte(ctx, 0x76, 4);
      break;
    }

    case IM: {
      check_arg_presence(ctx, arg1, 1);

      if (try_get_arg_8imm(ctx, arg1, &opc, &is_ref, -1) && (opc == 0) && !is_ref)
        render_2bytes(ctx, 0xED, 0x46, 8);
      else if (try_get_arg_8imm(ctx, arg1, &opc, &is_ref, -1) && (opc == 1) && !is_ref)
        render_2bytes(ctx, 0xED, 0x56, 8);
      else if (try_get_arg_8imm(ctx, arg1, &opc, &is_ref, -1) && (opc == 2) && !is_ref)
        render_2bytes(ctx, 0xED, 0x5E, 8);
      else
        ERR_UNEXPECTED_ARGUMENT(1);

      break;
    }

    case IN: {
      check_arg_presence(ctx, arg1, 1);
      check_arg_presence(ctx, arg2, 2);

      if (try_get_arg_accum(arg1)) {
        if (try_get_arg_8imm(ctx, arg2, &opc, &is_ref, section->curr_pc - section->start + 2) && is_ref) {
          check_integer_overflow(ctx, opc, 1);
          render_2bytes(ctx, 0xDB, opc, 11);
        } else if (try_get_arg_gpr8(arg2, &opc2, &is_ref) && is_ref && (opc2 == REG_C))
          render_2bytes(ctx, 0xED, 0x78, 12);
        else
          ERR_UNEXPECTED_ARGUMENT(2);
      } else if (try_get_arg_gpr8(arg1, &opc, &is_ref) && !is_ref) {
        if (try_get_arg_gpr8(arg2, &opc2, &is_ref) && is_ref && (opc2 == REG_C))
          render_2bytes(ctx, 0xED, 0x40 | (opc << 3), 12);
        else
          ERR_UNEXPECTED_ARGUMENT(2);
      } else if (try_get_arg_flags(arg1)) {
        if (try_get_arg_gpr8(arg2, &opc2, &is_ref) && is_ref && (opc2 == REG_C))
          render_2bytes(ctx, 0xED, 0x70, 12);
        else
          ERR_UNEXPECTED_ARGUMENT(2);
      } else {
        ERR_UNEXPECTED_ARGUMENT(1);
      }

      break;
    }

    case INC: {
      check_arg_presence(ctx, arg1, 1);

      if (try_get_arg_gpr8(arg1, &opc, &is_ref) && !is_ref)
        render_byte(ctx, 0x04 | (opc << 3), 4);
      else if (try_get_arg_hl(arg1, &is_ref) && is_ref)
        render_byte(ctx, 0x34, 6);
      else if (try_get_arg_index_offset8(ctx, arg1, &opc, &opc2, section->curr_pc - section->start + 2))
        render_3bytes(ctx, 0xDD | (opc << 5), 0x34, opc2, 23);
      else if (try_get_arg_qreg16(arg1, &opc2, &is_ref) && !is_ref)
        render_byte(ctx, 0x03 | (opc2 << 4), 6);
      else if (try_get_arg_index(arg1, &opc, &is_ref) && !is_ref)
        render_2bytes(ctx, 0xDD | (opc << 5), 0x23, 10);
      else if (try_get_arg_ixy_half(arg1, &opc, &opc2))
        render_2bytes(ctx, opc, 0x04 | (opc2 << 3), 8);
      else
        ERR_UNEXPECTED_ARGUMENT(1);

      break;
    }

    case IND: {
      render_2bytes(ctx, 0xED, 0xAA, 16);
      break;
    }

    case INDR: {
      render_2bytes(ctx, 0xED, 0xBA, 21);
      break;
    }

    case INI: {
      render_2bytes(ctx, 0xED, 0xA2, 16);
      break;
    }

    case INIR: {
      render_2bytes(ctx, 0xED, 0xB2, 21);
      break;
    }

    case JP: {
      int val;

      check_arg_presence(ctx, arg1, 1);

      if (num_args == 1) {
        int lsb=0, msb=0;

        if (try_get_arg_16imm(ctx, arg1, &lsb, &msb, &val, &is_ref, section->curr_pc - section->start + 1) && !is_ref) {
          check_integer_overflow(ctx, val, 2);
          render_3bytes(ctx, 0xC3, lsb, msb, 10);
        } else if (try_get_arg_hl(arg1, &is_ref) && is_ref)
          render_byte(ctx, 0xE9, 4);
        else if (try_get_arg_index(arg1, &lsb, &is_ref) && is_ref)
          render_2bytes(ctx, 0xDD | (lsb << 5), 0xE9, 8);
        else
          ERR_UNEXPECTED_ARGUMENT(1);
      } else if (num_args == 2) {
        int lsb, msb, cond;

        check_arg_presence(ctx, arg2, 2);

        if (try_get_arg_condition(arg1, &cond)) {
          if (try_get_arg_16imm(ctx, arg2, &lsb, &msb, &val, &is_ref, section->curr_pc - section->start + 1) && !is_ref) {
            check_integer_overflow(ctx, val, 2);
            render_3bytes(ctx, 0xC2 | (cond << 3), lsb, msb, 10);
          } else
            ERR_UNEXPECTED_ARGUMENT(2);
        } else {
          ERR_UNEXPECTED_ARGUMENT(1);
        }
      }

      break;
    }

    case JR: {
      int reladdr;

      check_arg_presence(ctx, arg1, 1);

      if (num_args == 1) {
        if (try_get_arg_reladdr(ctx, arg1, &reladdr, section->curr_pc - section->start + 1))
          render_2bytes(ctx, 0x18, reladdr, 12);
        else {
          check_reljump_overflow(ctx, reladdr, "JR");
          ERR_UNEXPECTED_ARGUMENT(1);
        }
      } else if (num_args == 2) {
        int cond;

        check_arg_presence(ctx, arg2, 2);

        if (try_get_arg_reladdr(ctx, arg2, &reladdr, section->curr_pc - section->start + 1)) {
          if (try_get_arg_condition(arg1, &cond) && (cond == COND_NZ))
            render_2bytes(ctx, 0x20, reladdr, 12);
          else if (try_get_arg_condition(arg1, &cond) && (cond == COND_Z))
            render_2bytes(ctx, 0x28, reladdr, 12);
          else if (try_get_arg_condition(arg1, &cond) && (cond == COND_NC))
            render_2bytes(ctx, 0x30, reladdr, 12);
          else if (try_get_arg_condition(arg1, &cond) && (cond == COND_C))
            render_2bytes(ctx, 0x38, reladdr, 12);
          else
            ERR_UNEXPECTED_ARGUMENT(2);
        } else {
          check_reljump_overflow(ctx, reladdr, "JR");
          ERR_UNEXPECTED_ARGUMENT(1);
        }
      }

      break;
    }

    case LD: {
      int i, b, i2, b2, opc3, val;

      check_arg_presence(ctx, arg1, 1);
      check_arg_presence(ctx, arg2, 2);

      if (try_get_arg_gpr8(arg1, &opc, &is_ref) && !is_ref) {
        if ((opc == REG_A) && try_get_arg_qreg16(arg2, &opc2, &is_ref) && is_ref && (opc2 == REG_BC))
          render_byte(ctx, 0x0A, 7);
        else if ((opc == REG_A) && try_get_arg_qreg16(arg2, &opc2, &is_ref) && is_ref && (opc2 == REG_DE))
          render_byte(ctx, 0x1A, 7);
        else if ((opc == REG_A) && try_get_arg_16imm(ctx, arg2, &opc3, &opc2, &val, &is_ref, section->curr_pc - section->start + 1) && is_ref) {
          check_integer_overflow(ctx, val, 2);
          render_3bytes(ctx, 0x3A, opc3, opc2, 13);
        } else if ((opc == REG_A) && try_get_arg_intreg(arg2))
          render_2bytes(ctx, 0xED, 0x57, 9);
        else if ((opc == REG_A) && try_get_arg_rfshreg(arg2))
          render_2bytes(ctx, 0xED, 0x5F, 9);
        else if (try_get_arg_gpr8(arg2, &opc2, &is_ref) && !is_ref)
          render_byte(ctx, 0x40 | (opc << 3) | opc2, 4);
        else if (try_get_arg_8imm(ctx, arg2, &opc2, &is_ref, section->curr_pc - section->start + 2) && !is_ref) {
          check_integer_overflow(ctx, opc2, 1);
          render_2bytes(ctx, 0x06 | (opc << 3), opc2, 7);
        } else if (try_get_arg_hl(arg2, &is_ref) && is_ref)
          render_byte(ctx, 0x46 | (opc << 3), 7);
        else if (try_get_arg_index_offset8(ctx, arg2, &i, &opc2, section->curr_pc - section->start + 2)) {
          check_integer_overflow(ctx, opc2, 1);
          render_3bytes(ctx, 0xDD | (i << 5), 0x46 | (opc << 3), opc2, 19);
        } else if ((opc == REG_A) && (try_get_arg_16imm(ctx, arg2, &opc, &opc2, &val, &is_ref, section->curr_pc - section->start + 1) && is_ref)) {
          check_integer_overflow(ctx, val, 2);
          render_3bytes(ctx, 0x3A, opc, opc2, 13);
        } else if (try_get_arg_ixy_half(arg2, &opc2, &opc3))
          render_2bytes(ctx, opc2, 0x40 | (opc << 3) | opc3, 8);
        else
          ERR_UNEXPECTED_ARGUMENT(2);
      } else if (try_get_arg_hl(arg1, &is_ref) && is_ref) {
        if (try_get_arg_gpr8(arg2, &opc, &is_ref) && !is_ref)
          render_byte(ctx, 0x70 | opc, 7);
        else if (try_get_arg_8imm(ctx, arg2, &opc, &is_ref, section->curr_pc - section->start + 1) && !is_ref) {
          check_integer_overflow(ctx, opc, 1);
          render_2bytes(ctx, 0x36, opc, 10);
        } else
          ERR_UNEXPECTED_ARGUMENT(2);
      } else if (try_get_arg_hl(arg1, &is_ref) && !is_ref) {
        if (try_get_arg_16imm(ctx, arg2, &opc, &opc2, &val, &is_ref, section->curr_pc - section->start + 1) && is_ref) {
          check_integer_overflow(ctx, val, 2);
          render_3bytes(ctx, 0x2A, opc, opc2, 16);
        } else if (try_get_arg_16imm(ctx, arg2, &opc, &opc2, &val, &is_ref, section->curr_pc - section->start + 1) && !is_ref) {
          check_integer_overflow(ctx, val, 2);
          render_3bytes(ctx, 0x01 | (REG_HL << 4), opc, opc2, 10);
        } else {
          ERR_UNEXPECTED_ARGUMENT(2);
        }
      } else if (try_get_arg_index_offset8(ctx, arg1, &i, &opc, section->curr_pc - section->start + 2)) {
        check_integer_overflow(ctx, opc, 1);
        if (try_get_arg_gpr8(arg2, &opc2, &is_ref) && !is_ref)
          render_3bytes(ctx, 0xDD | (i << 5), 0x70 | opc2, opc, 19);
        else if (try_get_arg_8imm(ctx, arg2, &opc2, &is_ref, section->curr_pc - section->start + 3) && !is_ref) {
          check_integer_overflow(ctx, opc2, 1);
          render_4bytes(ctx, 0xDD | (i << 5), 0x36, opc, opc2, 19);
        } else
          ERR_UNEXPECTED_ARGUMENT(2);
      } else if (try_get_arg_qreg16(arg1, &opc, &is_ref) && is_ref && (opc == REG_BC)) {
        if (try_get_arg_accum(arg2))
          render_byte(ctx, 0x02, 7);
        else
          ERR_UNEXPECTED_ARGUMENT(2);
      } else if (try_get_arg_qreg16(arg1, &opc, &is_ref) && is_ref && (opc == REG_DE)) {
        if (try_get_arg_accum(arg2))
          render_byte(ctx, 0x12, 7);
        else
          ERR_UNEXPECTED_ARGUMENT(2);
      } else if (try_get_arg_16imm(ctx, arg1, &opc, &opc2, &val, &is_ref, -1) && is_ref) {
        check_integer_overflow(ctx, val, 2);
        if (try_get_arg_accum(arg2))
          render_3bytes(ctx, 0x32, opc, opc2, 13);
        else if (try_get_arg_hl(arg2, &is_ref) && !is_ref)
          render_3bytes(ctx, 0x22, opc, opc2, 16);
        else if (try_get_arg_index(arg2, &opc3, &is_ref) && !is_ref)
          render_4bytes(ctx, 0xDD | (opc3 << 5), 0x22, opc, opc2, 20);
        else if (try_get_arg_qreg16(arg2, &opc3, &is_ref) && !is_ref)
          render_4bytes(ctx, 0xED, 0x43 | (opc3 << 4), opc, opc2, 20);
        else
          ERR_UNEXPECTED_ARGUMENT(2);
      } else if (try_get_arg_intreg(arg1)) {
        if (try_get_arg_accum(arg2))
          render_2bytes(ctx, 0xED, 0x47, 9);
        else
          ERR_UNEXPECTED_ARGUMENT(2);
      } else if (try_get_arg_rfshreg(arg1)) {
        if (try_get_arg_accum(arg2))
          render_2bytes(ctx, 0xED, 0x4F, 9);
        else
          ERR_UNEXPECTED_ARGUMENT(2);
      } else if (try_get_arg_qreg16(arg1, &opc, &is_ref) && !is_ref) {
        if ((opc == REG_SP) && try_get_arg_hl(arg2, &is_ref) && !is_ref)
          render_byte(ctx, 0xF9, 6);
        else if ((opc == REG_SP) && try_get_arg_index(arg2, &opc2, &is_ref) && !is_ref)
          render_2bytes(ctx, 0xDD | (opc2 << 5), 0xF9, 10);
        else if (try_get_arg_16imm(ctx, arg2, &opc2, &opc3, &val, &is_ref, section->curr_pc - section->start + 1) && !is_ref) {
          check_integer_overflow(ctx, val, 2);
          render_3bytes(ctx, 0x01 | (opc << 4), opc2, opc3, 10);
        } else if (try_get_arg_16imm(ctx, arg2, &opc2, &opc3, &val, &is_ref, section->curr_pc - section->start + 2) && is_ref) {
          check_integer_overflow(ctx, val, 2);
          render_4bytes(ctx, 0xED, 0x4B | (opc << 4), opc2, opc3, 20);
        } else
          ERR_UNEXPECTED_ARGUMENT(2);
      } else if (try_get_arg_index(arg1, &opc, &is_ref) && !is_ref) {
        if (try_get_arg_16imm(ctx, arg2, &opc2, &opc3, &val, &is_ref, section->curr_pc - section->start + 2) && !is_ref) {
          check_integer_overflow(ctx, val, 2);
          render_4bytes(ctx, 0xDD | (opc << 5), 0x21, opc2, opc3, 14);
        } else if (try_get_arg_16imm(ctx, arg2, &opc2, &opc3, &val, &is_ref, section->curr_pc - section->start + 2) && is_ref) {
          check_integer_overflow(ctx, val, 2);
          render_4bytes(ctx, 0xDD | (opc << 5), 0x2A, opc2, opc3, 20);
        } else
          ERR_UNEXPECTED_ARGUMENT(2);
      } else if (try_get_arg_ixy_half(arg1, &opc, &opc2)) {
        int second_arg, pfx2;
        if (try_get_arg_gpr8(arg2, &second_arg, &is_ref) && !is_ref) {
          render_2bytes(ctx, opc, 0x40 | (opc2 << 3) | second_arg, 8);
        } else if (try_get_arg_8imm(ctx, arg2, &second_arg, &is_ref, section->curr_pc - section->start + 2) && !is_ref) {
          check_integer_overflow(ctx, second_arg, 1);
          render_3bytes(ctx, opc, 0x06 | (opc2 << 3), second_arg, 11);
        } else if (try_get_arg_ixy_half(arg2, &pfx2, &opc3)) {
          render_2bytes(ctx, opc, 0x40 | (opc2 << 3) | opc3, 8);
        } else
          ERR_UNEXPECTED_ARGUMENT(2);
      } else
        ERR_UNEXPECTED_ARGUMENT(1);

      break;
    }

    case LDD: {
      render_2bytes(ctx, 0xED, 0xA8, 16);
      break;
    }

    case LDDR: {
      render_2bytes(ctx, 0xED, 0xB8, 21);
      break;
    }

    case LDI: {
      render_2bytes(ctx, 0xED, 0xA0, 16);
      break;
    }

    case LDIR: {
      render_2bytes(ctx, 0xED, 0xB0, 21);
      break;
    }

    case NEG: {
      render_2bytes(ctx, 0xED, 0x44, 8);
      break;
    }

    case NOP: {
      render_byte(ctx, 0, 4);
      break;
    }

    case OR: {
      check_arg_presence(ctx, arg1, 1);

      if (try_get_arg_gpr8(arg1, &opc, &is_ref) && !is_ref)
        render_byte(ctx, 0xB0 | opc, 4);
      else if (try_get_arg_8imm(ctx, arg1, &opc, &is_ref, section->curr_pc - section->start + 1) && !is_ref) {
        check_integer_overflow(ctx, opc, 1);
        render_2bytes(ctx, 0xF6, opc, 7);
      } else if (try_get_arg_hl(arg1, &is_ref) && is_ref)
        render_byte(ctx, 0xB6, 7);
      else if (try_get_arg_index_offset8(ctx, arg1, &opc, &opc2, section->curr_pc - section->start + 2)) {
        check_integer_overflow(ctx, opc2, 1);
        render_3bytes(ctx, 0xDD | (opc << 5), 0xB6, opc2, 19);
      } else if (try_get_arg_ixy_half(arg1, &opc, &opc2))
        render_2bytes(ctx, opc, 0xB0 | opc2, 8);
      else
        ERR_UNEXPECTED_ARGUMENT(1);

      break;
    }

    case OUT: {
      check_arg_presence(ctx, arg1, 1);
      check_arg_presence(ctx, arg2, 2);

      if (try_get_arg_8imm(ctx, arg1, &opc, &is_ref, section->curr_pc - section->start + 1) && is_ref) {
        check_integer_overflow(ctx, opc, 1);
        if (try_get_arg_accum(arg2))
          render_2bytes(ctx, 0xD3, opc, 11);
        else
          ERR_UNEXPECTED_ARGUMENT(2);
      } else if (try_get_arg_gpr8(arg1, &opc, &is_ref) && is_ref && (opc == REG_C)) {
        if (try_get_arg_gpr8(arg2, &opc2, &is_ref) && !is_ref)
          render_2bytes(ctx, 0xED, 0x41 | (opc2 << 3), 12);
        else if (try_get_arg_8imm(ctx, arg2, &opc2, &is_ref, -1) && !is_ref && (opc2 == 0))
          render_2bytes(ctx, 0xED, 0x71, 12);
        else
          ERR_UNEXPECTED_ARGUMENT(2);
      } else {
        ERR_UNEXPECTED_ARGUMENT(1);
      }

      break;
    }

    case OUTD: {
      render_2bytes(ctx, 0xED, 0xAB, 16);
      break;
    }

    case OTDR: {
      render_2bytes(ctx, 0xED, 0xBB, 21);
      break;
    }

    case OUTI: {
      render_2bytes(ctx, 0xED, 0xA3, 16);
      break;
    }

    case OTIR: {
      render_2bytes(ctx, 0xED, 0xB3, 21);
      break;
    }

    case POP: {
      check_arg_presence(ctx, arg1, 1);

      if (try_get_arg_preg16(arg1, &opc, &is_ref) && !is_ref)
        render_byte(ctx, 0xC1 | (opc << 4), 10);
      else if (try_get_arg_index(arg1, &opc, &is_ref) && !is_ref)
        render_2bytes(ctx, 0xDD | (opc << 5), 0xE1, 14);
      else
        ERR_UNEXPECTED_ARGUMENT(1);

      break;
    }

    case PUSH: {
      check_arg_presence(ctx, arg1, 1);

      if (try_get_arg_preg16(arg1, &opc, &is_ref) && !is_ref)
        render_byte(ctx, 0xC5 | (opc << 4), 11);
      else if (try_get_arg_index(arg1, &opc, &is_ref) && !is_ref)
        render_2bytes(ctx, 0xDD | (opc << 5), 0xE5, 15);
      else
        ERR_UNEXPECTED_ARGUMENT(1);

      break;
    }

    case RES: {
      int opc3;

      check_arg_presence(ctx, arg1, 1);
      check_arg_presence(ctx, arg2, 2);

      if (try_get_arg_bitnum(arg1, &opc)) {
        if (try_get_arg_gpr8(arg2, &opc2, &is_ref) && !is_ref)
          render_2bytes(ctx, 0xCB, 0x80 | (opc << 3) | opc2, 8);
        else if (try_get_arg_hl(arg2, &is_ref) && is_ref)
          render_2bytes(ctx, 0xCB, 0x86 | (opc << 3), 15);
        else if (try_get_arg_index_offset8(ctx, arg2, &opc2, &opc3, section->curr_pc - section->start + 2)) {
          check_integer_overflow(ctx, opc3, 1);
          render_4bytes(ctx, 0xDD | (opc2 << 5), 0xCB, opc3, 0x86 | (opc << 3), 23);
        } else
          ERR_UNEXPECTED_ARGUMENT(2);
      } else {
        check_bitnum(ctx, opc, "RES");
        ERR_UNEXPECTED_ARGUMENT(1);
      }

      break;
    }

    case RET: {
      if (num_args == 0) {
        render_byte(ctx, 0xC9, 10);
      } else if (num_args == 1) {
        check_arg_presence(ctx, arg1, 1);

        if (try_get_arg_condition(arg1, &opc))
          render_byte(ctx, 0xC0 | (opc << 3), 11);
        else
          ERR_UNEXPECTED_ARGUMENT(1);
      }

      break;
    }

    case RETI: {
      render_2bytes(ctx, 0xED, 0x4D, 14);
      break;
    }

    case RETN: {
      render_2bytes(ctx, 0xED, 0x45, 14);
      break;
    }

    case RLA: {
      render_byte(ctx, 0x17, 4);
      break;
    }

    case RL: {
      check_arg_presence(ctx, arg1, 1);

      if (try_get_arg_gpr8(arg1, &opc, &is_ref) && !is_ref)
        render_2bytes(ctx, 0xCB, 0x10 | opc, 8);
      else if (try_get_arg_hl(arg1, &is_ref) && is_ref)
        render_2bytes(ctx, 0xCB, 0x16, 15);
      else if (try_get_arg_index_offset8(ctx, arg1, &opc, &opc2, section->curr_pc - section->start + 2)) {
        check_integer_overflow(ctx, opc2, 1);
        render_4bytes(ctx, 0xDD | (opc << 5), 0xCB, opc2, 0x16, 23);
      } else
        ERR_UNEXPECTED_ARGUMENT(1);

      break;
    }

    case RLCA: {
      render_byte(ctx, 0x07, 4);
      break;
    }

    case RLC: {
      check_arg_presence(ctx, arg1, 1);

      if (try_get_arg_gpr8(arg1, &opc, &is_ref) && !is_ref)
        render_2bytes(ctx, 0xCB, 0x00 | opc, 8);
      else if (try_get_arg_hl(arg1, &is_ref) && is_ref)
        render_2bytes(ctx, 0xCB, 0x06, 15);
      else if (try_get_arg_index_offset8(ctx, arg1, &opc, &opc2, section->curr_pc - section->start + 2)) {
        check_integer_overflow(ctx, opc2, 1);
        render_4bytes(ctx, 0xDD | (opc << 5), 0xCB, opc2, 0x06, 23);
      } else
        ERR_UNEXPECTED_ARGUMENT(1);

      break;
    }

    case RLD: {
      render_2bytes(ctx, 0xED, 0x6F, 18);
      break;
    }

    case RRA: {
      render_byte(ctx, 0x1F, 4);
      break;
    }

    case RR: {
      check_arg_presence(ctx, arg1, 1);

      if (try_get_arg_gpr8(arg1, &opc, &is_ref) && !is_ref)
        render_2bytes(ctx, 0xCB, 0x18 | opc, 8);
      else if (try_get_arg_hl(arg1, &is_ref) && is_ref)
        render_2bytes(ctx, 0xCB, 0x1E, 15);
      else if (try_get_arg_index_offset8(ctx, arg1, &opc, &opc2, section->curr_pc - section->start + 2)) {
        check_integer_overflow(ctx, opc2, 1);
        render_4bytes(ctx, 0xDD | (opc << 5), 0xCB, opc2, 0x1E, 23);
      } else
        ERR_UNEXPECTED_ARGUMENT(1);

      break;
    }

    case RRCA: {
      render_byte(ctx, 0x0F, 4);
      break;
    }

    case RRC: {
      check_arg_presence(ctx, arg1, 1);

      if (try_get_arg_gpr8(arg1, &opc, &is_ref) && !is_ref)
        render_2bytes(ctx, 0xCB, 0x08 | opc, 8);
      else if (try_get_arg_hl(arg1, &is_ref) && is_ref)
        render_2bytes(ctx, 0xCB, 0x0E, 15);
      else if (try_get_arg_index_offset8(ctx, arg1, &opc, &opc2, section->curr_pc - section->start + 2)) {
        check_integer_overflow(ctx, opc2, 1);
        render_4bytes(ctx, 0xDD | (opc << 5), 0xCB, opc2, 0x0E, 23);
      } else
        ERR_UNEXPECTED_ARGUMENT(1);

      break;
    }

    case RRD: {
      render_2bytes(ctx, 0xED, 0x67, 18);
      break;
    }

    case RST: {
      int val;

      check_arg_presence(ctx, arg1, 1);

      if (try_get_arg_rstaddr(arg1, &opc, &val))
        render_byte(ctx, 0xC7 | (opc << 3), 11);
      else {
        report_error(ctx, "invalid value 0x%x for RST",
          val);
      }

      break;
    }

    case SBC: {
      check_arg_presence(ctx, arg1, 1);
      check_arg_presence(ctx, arg2, 2);

      if (try_get_arg_accum(arg1)) {
        if (try_get_arg_gpr8(arg2, &opc, &is_ref) && !is_ref)
          render_byte(ctx, 0x98 | opc, 4);
        else if (try_get_arg_8imm(ctx, arg2, &opc, &is_ref, section->curr_pc - section->start + 1) && !is_ref) {
          check_integer_overflow(ctx, opc, 1);
          render_2bytes(ctx, 0xDE, opc, 7);
        } else if (try_get_arg_hl(arg2, &is_ref) && is_ref)
          render_byte(ctx, 0x9E, 7);
        else if (try_get_arg_index_offset8(ctx, arg2, &opc, &opc2, section->curr_pc - section->start + 2)) {
          check_integer_overflow(ctx, opc2, 1);
          render_3bytes(ctx, 0xDD | (opc << 5), 0x9E, opc2, 19);
        } else if (try_get_arg_ixy_half(arg2, &opc, &opc2))
          render_2bytes(ctx, opc, 0x98 | opc2, 8);
        else
          ERR_UNEXPECTED_ARGUMENT(2);
      } else if (try_get_arg_hl(arg1, &is_ref) && !is_ref) {
        if (try_get_arg_qreg16(arg2, &opc, &is_ref) && !is_ref)
          render_2bytes(ctx, 0xED, 0x42 | (opc << 4), 15);
        else
          ERR_UNEXPECTED_ARGUMENT(2);
      } else {
        ERR_UNEXPECTED_ARGUMENT(1);
      }

      break;
    }

    case SCF: {
      render_byte(ctx, 0x37, 4);
      break;
    }

    case SET: {
      int opc3;

      check_arg_presence(ctx, arg1, 1);
      check_arg_presence(ctx, arg2, 1);

      if (try_get_arg_bitnum(arg1, &opc)) {
        if (try_get_arg_gpr8(arg2, &opc2, &is_ref) && !is_ref)
          render_2bytes(ctx, 0xCB, 0xC0 | (opc << 3) | opc2, 8);
        else if (try_get_arg_hl(arg2, &is_ref) && is_ref)
          render_2bytes(ctx, 0xCB, 0xC6 | (opc << 3), 15);
        else if (try_get_arg_index_offset8(ctx, arg2, &opc2, &opc3, section->curr_pc - section->start + 2)) {
          check_integer_overflow(ctx, opc3, 1);
          render_4bytes(ctx, 0xDD | (opc2 << 5), 0xCB, opc3, 0xC6 | (opc << 3), 23);
        } else
          ERR_UNEXPECTED_ARGUMENT(2);
      } else {
        check_bitnum(ctx, opc, "SET");
        ERR_UNEXPECTED_ARGUMENT(1);
      }

      break;
    }

    case SLA: {
      check_arg_presence(ctx, arg1, 1);

      if (try_get_arg_gpr8(arg1, &opc, &is_ref) && !is_ref)
        render_2bytes(ctx, 0xCB, 0x20 | opc, 8);
      else if (try_get_arg_hl(arg1, &is_ref) && is_ref)
        render_2bytes(ctx, 0xCB, 0x26, 15);
      else if (try_get_arg_index_offset8(ctx, arg1, &opc, &opc2, section->curr_pc - section->start + 2)) {
        check_integer_overflow(ctx, opc2, 1);
        render_4bytes(ctx, 0xDD | (opc << 5), 0xCB, opc2, 0x26, 23);
      } else
        ERR_UNEXPECTED_ARGUMENT(1);

      break;
    }

    case SRA: {
      check_arg_presence(ctx, arg1, 1);

      if (try_get_arg_gpr8(arg1, &opc, &is_ref) && !is_ref)
        render_2bytes(ctx, 0xCB, 0x28 | opc, 8);
      else if (try_get_arg_hl(arg1, &is_ref) && is_ref)
        render_2bytes(ctx, 0xCB, 0x2E, 15);
      else if (try_get_arg_index_offset8(ctx, arg1, &opc, &opc2, section->curr_pc - section->start + 2)) {
        check_integer_overflow(ctx, opc2, 1);
        render_4bytes(ctx, 0xDD | (opc << 5), 0xCB, opc2, 0x2E, 23);
      } else
        ERR_UNEXPECTED_ARGUMENT(1);

      break;
    }

    case SLL: {
      check_arg_presence(ctx, arg1, 1);

      if (try_get_arg_gpr8(arg1, &opc, &is_ref) && !is_ref)
        render_2bytes(ctx, 0xCB, 0x30 | opc, 8);
      else if (try_get_arg_hl(arg1, &is_ref) && is_ref)
        render_2bytes(ctx, 0xCB, 0x36, 15);
      else if (try_get_arg_index_offset8(ctx, arg1, &opc, &opc2, section->curr_pc - section->start + 2)) {
        check_integer_overflow(ctx, opc2, 1);
        render_4bytes(ctx, 0xDD | (opc << 5), 0xCB, opc2, 0x36, 23);
      } else
        ERR_UNEXPECTED_ARGUMENT(1);

      break;
    }

    case SRL: {
      check_arg_presence(ctx, arg1, 1);

      if (try_get_arg_gpr8(arg1, &opc, &is_ref) && !is_ref)
        render_2bytes(ctx, 0xCB, 0x38 | opc, 8);
      else if (try_get_arg_hl(arg1, &is_ref) && is_ref)
        render_2bytes(ctx, 0xCB, 0x3E, 15);
      else if (try_get_arg_index_offset8(ctx, arg1, &opc, &opc2, section->curr_pc - section->start + 2)) {
        check_integer_overflow(ctx, opc2, 1);
        render_4bytes(ctx, 0xDD | (opc << 5), 0xCB, opc2, 0x3E, 23);
      } else
        ERR_UNEXPECTED_ARGUMENT(1);

      break;
    }

    case SUB: {
      check_arg_presence(ctx, arg1, 1);

      if (try_get_arg_gpr8(arg1, &opc, &is_ref) && !is_ref)
        render_byte(ctx, 0x90 | opc, 4);
      else if (try_get_arg_8imm(ctx, arg1, &opc, &is_ref, section->curr_pc - section->start + 1) && !is_ref) {
        check_integer_overflow(ctx, opc, 1);
        render_2bytes(ctx, 0xD6, opc, 7);
      } else if (try_get_arg_hl(arg1, &is_ref) && is_ref)
        render_byte(ctx, 0x96, 7);
      else if (try_get_arg_index_offset8(ctx, arg1, &opc, &opc2, section->curr_pc - section->start + 2)) {
        check_integer_overflow(ctx, opc2, 1);
        render_3bytes(ctx, 0xDD | (opc << 5), 0x96, opc2, 19);
      } else if (try_get_arg_ixy_half(arg1, &opc, &opc2))
        render_2bytes(ctx, opc, 0x90 | opc2, 8);
      else
        ERR_UNEXPECTED_ARGUMENT(1);

      break;
    }

    case XOR: {
      check_arg_presence(ctx, arg1, 1);

      if (try_get_arg_gpr8(arg1, &opc, &is_ref) && !is_ref)
        render_byte(ctx, 0xA8 | opc, 4);
      else if (try_get_arg_8imm(ctx, arg1, &opc, &is_ref, section->curr_pc - section->start + 1) && !is_ref) {
        check_integer_overflow(ctx, opc, 1);
        render_2bytes(ctx, 0xEE, opc, 7);
      } else if (try_get_arg_hl(arg1, &is_ref) && is_ref)
        render_byte(ctx, 0xAE, 7);
      else if (try_get_arg_index_offset8(ctx, arg1, &opc, &opc2, section->curr_pc - section->start + 2)) {
        check_integer_overflow(ctx, opc2, 1);
        render_3bytes(ctx, 0xDD | (opc << 5), 0xAE, opc2, 19);
      } else if (try_get_arg_ixy_half(arg1, &opc, &opc2))
        render_2bytes(ctx, opc, 0xA8 | opc2, 8);
      else
        ERR_UNEXPECTED_ARGUMENT(1);

      break;
    }

    default:
      // unreachable
      abort();
      break;
  }
}

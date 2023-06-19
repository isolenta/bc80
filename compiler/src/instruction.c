#include <stdio.h>
#include <stdarg.h>
#include <string.h>

#include "dynarray.h"
#include "hashmap.h"
#include "parse.h"
#include "common.h"
#include "compile.h"
#include "render.h"

#include "instr.inc.c"

static bool contains_reserved_ids(parse_node *node) {
  if (node == NULL)
    return false;

  if (node->type == NODE_ID) {
    ID *id = (ID *)node;

    for (int i = 0;; i++) {
      char *rsvname = RESERVED_IDENTS[i];
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

static inline bool check_id_name(ID *id, char *name) {
  return strcasecmp(id->name, name) == 0;
}

#define CHECK_ARG_IS_ID(_n_)  \
  if ((_n_)->type != NODE_ID) \
    return false;             \
  ID *id = (ID *)(_n_)

static bool get_arg_accum(parse_node *node) {
  CHECK_ARG_IS_ID(node);
  return check_id_name(id, "a");
}

static bool get_arg_flags(parse_node *node) {
  CHECK_ARG_IS_ID(node);
  return check_id_name(id, "f");
}

static bool get_arg_intreg(parse_node *node) {
  CHECK_ARG_IS_ID(node);
  return check_id_name(id, "i");
}

static bool get_arg_rfshreg(parse_node *node) {
  CHECK_ARG_IS_ID(node);
  return check_id_name(id, "r");
}

static bool get_arg_gpr8(parse_node *node, int *reg_opcode, bool *is_ref) {
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

  *is_ref = id->is_ref;

  return true;
}

static bool get_arg_hl(parse_node *node, bool *is_ref) {
  CHECK_ARG_IS_ID(node);

  if (check_id_name(id, "hl")) {
    *is_ref = id->is_ref;
    return true;
  }

  return false;
}

static bool get_arg_qreg16(parse_node *node, int *reg_opcode, bool *is_ref) {
  CHECK_ARG_IS_ID(node);

  if (check_id_name(id, "bc"))
    *reg_opcode = 0x0;
  else if (check_id_name(id, "de"))
    *reg_opcode = 0x1;
  else if (check_id_name(id, "hl"))
    *reg_opcode = 0x2;
  else if (check_id_name(id, "ix"))
    *reg_opcode = 0x2;
  else if (check_id_name(id, "iy"))
    *reg_opcode = 0x2;
  else if (check_id_name(id, "sp"))
    *reg_opcode = 0x3;
  else
    return false;

  *is_ref = id->is_ref;

  return true;
}

static bool get_arg_preg16(parse_node *node, int *reg_opcode, bool *is_ref) {
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

  *is_ref = id->is_ref;

  return true;
}

static bool get_arg_af(parse_node *node) {
  CHECK_ARG_IS_ID(node);
  return check_id_name(id, "af");
}

static bool get_arg_af_shadow(parse_node *node) {
  CHECK_ARG_IS_ID(node);
  return check_id_name(id, "af'");
}

static bool get_arg_halfindex(parse_node *node, int *i, int *b) {
  CHECK_ARG_IS_ID(node);

  if (check_id_name(id, "ixh")) {
    *i = 0; *b = 0;
  } else if (check_id_name(id, "ixl")) {
    *i = 0; *b = 1;
  } else if (check_id_name(id, "iyh")) {
    *i = 1; *b = 0;
  } else if (check_id_name(id, "iyl")) {
    *i = 1; *b = 1;
  } else {
    return false;
  }
  return true;
}

static bool get_arg_index(parse_node *node, int *i, bool *is_ref) {
  CHECK_ARG_IS_ID(node);

  if (check_id_name(id, "ix"))
    *i = REG_IX;
  else if (check_id_name(id, "iy"))
    *i = REG_IY;
  else
    return false;

  *is_ref = id->is_ref;

  return true;
}


static bool get_arg_8imm(compile_ctx_t *ctx, parse_node *node, int *ival, bool *is_ref, int imm_pos) {
  if (node->type == NODE_LITERAL) {
    LITERAL *l = (LITERAL *)node;
    if (l->kind == INT) {
      *ival = l->ival;
      *is_ref = l->is_ref;
      return true;
    }
  } else if ((imm_pos != -1) && !contains_reserved_ids(node)) {
    // will patch operand position with literal value at 2nd pass

    if (node->type == NODE_ID)
      *is_ref = ((ID *)node)->is_ref;
    else if (node->type == NODE_EXPR)
      *is_ref = ((EXPR *)node)->is_ref;

    *ival = 0;
    register_fwd_lookup(ctx, node, imm_pos, 1, false, false, 0);

    return true;
  }
  return false;
}

static bool get_arg_16imm(compile_ctx_t *ctx,
                          parse_node *node,
                          int *lsb, int *msb,
                          bool *is_ref,
                          int imm_pos) {
  if (node->type == NODE_LITERAL) {
    LITERAL *l = (LITERAL *)node;
    if (l->kind == INT) {
      *lsb = l->ival & 0xFF;
      *msb = (l->ival >> 8) & 0xFF;
      *is_ref = l->is_ref;
      return true;
    }
  } else if ((imm_pos != -1) && !contains_reserved_ids(node)) {
    // will patch operand position with literal value at 2nd pass

    if (node->type == NODE_ID)
      *is_ref = ((ID *)node)->is_ref;
    else if (node->type == NODE_EXPR)
      *is_ref = ((EXPR *)node)->is_ref;

    *lsb = 0; *msb = 0;
    register_fwd_lookup(ctx, node, imm_pos, 2, false, false, 0);

    return true;
  }
  return false;
}

static bool get_arg_index_offset8(compile_ctx_t *ctx, parse_node *node, int *idx, int *offset, int imm_pos) {
  if (node->type != NODE_EXPR)
    return false;

  EXPR *expr = (EXPR *)node;
  if (!expr->is_ref)
    return false;

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
    register_fwd_lookup(ctx, node, imm_pos, 1, (expr->kind == BINARY_MINUS), false, 0);

    return true;
  }

  return false;
}

static bool get_arg_bitnum(parse_node *node, int *bitnum) {
  bool is_ref;

  // get_arg_8imm() doesn't need context for non-forward bitnum resolution so it's safe to pass NULL here
  bool result = get_arg_8imm(NULL, node, bitnum, &is_ref, -1);
  if ((*bitnum >= 0) && (*bitnum <= 7) && !is_ref)
    return result;
  return false;
}

static bool get_arg_condition(parse_node *node, int *cond) {
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

static bool get_arg_reladdr(compile_ctx_t *ctx, parse_node *node, int *reladdr, int imm_pos) {
  int lsb, msb;
  section_ctx_t *section = get_current_section(ctx);

  if (node->type == NODE_LITERAL) {
    LITERAL *l = (LITERAL *)node;
    if (l->is_ref)
      return false;

    if (l->kind == INT) {
      uint16_t addr = l->ival;
      *reladdr = section->curr_pc - addr;
      if ((*reladdr >= -128) && (*reladdr <= 127))
        return true;
      else
        report_warning(ctx, "only relative addresses -128..127 are allowed (got %d)\n", *reladdr);
    }
  } else if ((imm_pos != -1) && !contains_reserved_ids(node)) {
    // will patch operand position with literal value at 2nd pass

    *reladdr = 0;
    register_fwd_lookup(ctx, node, imm_pos, 1, false, true, section->curr_pc);

    return true;
  }
  return false;
}

static bool get_arg_rstaddr(parse_node *node, int *rstcode) {
  bool is_ref;
  int ival;

  // get_arg_8imm() doesn't need context for non-forward rstcode resolution so it's safe to pass NULL here
  if (get_arg_8imm(NULL, node, &ival, &is_ref, -1) && !is_ref) {
    switch (ival) {
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

void compile_instruction(compile_ctx_t *ctx, char *name, LIST *args) {
  section_ctx_t *section = get_current_section(ctx);

  #define ERR_UNEXPECTED_ARGUMENT(n) do { \
    report_error(ctx, "[%s@%d] unexpected argument %d\n", __FILE__, __LINE__, (n)); \
  } while(0)

  // printf("\x1b[32m%s\x1b[0m ", name);

  // get instruction ordinal id
  MnemonicEnum i_mnemonic = MAX_MNEMONIC_ID;
  for (int i = 0; i < MAX_MNEMONIC_ID; i++) {
    if (strcasecmp(name, MnemonicStrings[i]) == 0)
      i_mnemonic = (MnemonicEnum)i;
  }

  if (i_mnemonic == MAX_MNEMONIC_ID) {
    report_error(ctx, "no such instruction %s\n", name);
    return;
  }

  // check number of arguments
  int num_args = (args && args->list) ? dynarray_length(args->list) : 0;
  int chk_mask = 0;

  if (num_args == 0)
    chk_mask = NARGS_0;
  else if (num_args == 1)
    chk_mask = NARGS_1;
  else if (num_args == 2)
    chk_mask = NARGS_2;

  if (!(NUM_ARGS[(int)i_mnemonic] & chk_mask))
    report_error(ctx, "invalid number of arguments %d\n", num_args);

  // evaluate arguments expressions
  parse_node *arg1 = NULL;
  parse_node *arg2 = NULL;

  if (args && args->list && (dynarray_length(args->list) > 0))
    arg1 = dinitial(args->list);

  if (args && args->list && (dynarray_length(args->list) > 1))
    arg2 = dsecond(args->list);

  if (args && args->list && (dynarray_length(args->list) > 2))
    report_error(ctx, "invalid number of arguments %d\n", dynarray_length(args->list));

  bool is_ref;
  int opc, opc2;

  // compile it
  switch (i_mnemonic) {
    case ADC: {
      if (get_arg_accum(arg1)) {
        if (get_arg_gpr8(arg2, &opc, &is_ref) && !is_ref)
          render_byte(ctx, 0x88 | opc);
        else if (get_arg_halfindex(arg2, &opc, &opc2))
          render_2bytes(ctx, 0xDD | (opc << 5), 0x8C | opc2);
        else if (get_arg_8imm(ctx, arg2, &opc, &is_ref, section->curr_pc + 2) && !is_ref)
          render_2bytes(ctx, 0xCE, opc);
        else if (get_arg_hl(arg2, &is_ref) && is_ref)
          render_byte(ctx, 0x8E);
        else if (get_arg_index_offset8(ctx, arg2, &opc, &opc2, section->curr_pc + 2))
          render_3bytes(ctx, 0xDD | (opc << 5), 0x8E, opc2);
        else
          ERR_UNEXPECTED_ARGUMENT(2);
      } else if (get_arg_hl(arg1, &is_ref) && !is_ref) {
        if (get_arg_qreg16(arg2, &opc, &is_ref) && !is_ref)
          render_2bytes(ctx, 0xED, 0x4A | (opc << 4));
        else
          ERR_UNEXPECTED_ARGUMENT(2);
      } else {
        ERR_UNEXPECTED_ARGUMENT(1);
      }

      break;
    }

    case ADD: {
      if (get_arg_accum(arg1)) {
        if (get_arg_gpr8(arg2, &opc, &is_ref) && !is_ref)
          render_byte(ctx, 0x80 | opc);
        else if (get_arg_halfindex(arg2, &opc, &opc2))
          render_2bytes(ctx, 0xDD | (opc << 5), 0x84 | opc2);
        else if (get_arg_8imm(ctx, arg2, &opc, &is_ref, section->curr_pc + 2) && !is_ref)
          render_2bytes(ctx, 0xC6, opc);
        else if (get_arg_hl(arg2, &is_ref) && is_ref)
          render_byte(ctx, 0x86);
        else if (get_arg_index_offset8(ctx, arg2, &opc, &opc2, section->curr_pc + 2))
          render_3bytes(ctx, 0xDD | (opc << 5), 0x86, opc2);
        else
          ERR_UNEXPECTED_ARGUMENT(2);
      } else if (get_arg_hl(arg1, &is_ref) && !is_ref) {
        if (get_arg_qreg16(arg2, &opc, &is_ref) && !is_ref)
          render_byte(ctx, 0x09 | (opc << 4));
        else
          ERR_UNEXPECTED_ARGUMENT(2);
      } else if (get_arg_index(arg1, &opc, &is_ref) && !is_ref) {
        if (get_arg_qreg16(arg2, &opc2, &is_ref) && !is_ref)
          render_2bytes(ctx, 0xDD | (opc << 5), 0x09 | (opc2 << 4));
        else
          ERR_UNEXPECTED_ARGUMENT(2);
      } else {
        ERR_UNEXPECTED_ARGUMENT(1);
      }

      break;
    }

    case AND: {
      if (get_arg_gpr8(arg1, &opc, &is_ref) && !is_ref)
        render_byte(ctx, 0xA0 | opc);
      else if (get_arg_halfindex(arg1, &opc, &opc2))
        render_2bytes(ctx, 0xDD | (opc << 5), 0xA4 | opc2);
      else if (get_arg_8imm(ctx, arg1, &opc, &is_ref, section->curr_pc + 2) && !is_ref)
        render_2bytes(ctx, 0xE6, opc);
      else if (get_arg_hl(arg1, &is_ref) && is_ref)
        render_byte(ctx, 0xA6);
      else if (get_arg_index_offset8(ctx, arg1, &opc, &opc2, section->curr_pc + 2))
        render_3bytes(ctx, 0xDD | (opc << 5), 0xA6, opc2);
      else
        ERR_UNEXPECTED_ARGUMENT(1);

      break;
    }

    case BIT: {
      int b;

      if (get_arg_bitnum(arg1, &b)) {
        bool is_ref;
        if (get_arg_gpr8(arg2, &opc, &is_ref) && !is_ref)
          render_2bytes(ctx, 0xCB, 0x40 | (b << 3) | opc);
        else if (get_arg_hl(arg2, &is_ref) && is_ref)
          render_2bytes(ctx, 0xCB, 0x46 | (b << 3));
        else if (get_arg_index_offset8(ctx, arg1, &opc, &opc2, section->curr_pc + 2))
          render_4bytes(ctx, 0xDD | (opc << 5), 0xCB, opc2, 0x40 | (b << 3));
        else
          ERR_UNEXPECTED_ARGUMENT(2);
      } else {
        ERR_UNEXPECTED_ARGUMENT(1);
      }

      break;
    }

    case CALL: {
      if (num_args == 1) {
        int lsb, msb;

        if (get_arg_16imm(ctx, arg1, &lsb, &msb, &is_ref, section->curr_pc - section->start + 1) && !is_ref)
          render_3bytes(ctx, 0xCD, lsb, msb);
        else
          ERR_UNEXPECTED_ARGUMENT(1);
      } else if (num_args == 2) {
        int cond, lsb, msb;

        if (get_arg_condition(arg1, &cond)) {
          if (get_arg_16imm(ctx, arg2, &lsb, &msb, &is_ref, section->curr_pc - section->start + 1) && !is_ref)
            render_3bytes(ctx, 0xC4 | (cond << 3), lsb, msb);
          else
            ERR_UNEXPECTED_ARGUMENT(2);
        } else {
          ERR_UNEXPECTED_ARGUMENT(1);
        }
      }

      break;
    }

    case CCF: {
      render_byte(ctx, 0x3F);
      break;
    }

    case CP: {
      if (get_arg_gpr8(arg1, &opc, &is_ref) && !is_ref)
        render_byte(ctx, 0xB8 | opc);
      else if (get_arg_halfindex(arg1, &opc, &opc2))
        render_2bytes(ctx, 0xDD | (opc << 5), 0xBC | opc2);
      else if (get_arg_8imm(ctx, arg1, &opc, &is_ref, section->curr_pc - section->start + 2) && !is_ref)
        render_2bytes(ctx, 0xFE, opc);
      else if (get_arg_hl(arg1, &is_ref) && is_ref)
        render_byte(ctx, 0xBE);
      else if (get_arg_index_offset8(ctx, arg1, &opc, &opc2, section->curr_pc - section->start + 2))
        render_3bytes(ctx, 0xDD | (opc << 5), 0xBE, opc2);
      else
        ERR_UNEXPECTED_ARGUMENT(1);

      break;
    }

    case CPD: {
      render_2bytes(ctx, 0xED, 0xA9);
      break;
    }

    case CPDR: {
      render_2bytes(ctx, 0xED, 0xB9);
      break;
    }

    case CPI: {
      render_2bytes(ctx, 0xED, 0xA1);
      break;
    }

    case CPIR: {
      render_2bytes(ctx, 0xED, 0xB1);
      break;
    }

    case CPL: {
      render_byte(ctx, 0x2F);
      break;
    }

    case DAA: {
      render_byte(ctx, 0x27);
      break;
    }

    case DEC: {
      if (get_arg_gpr8(arg1, &opc, &is_ref) && !is_ref)
        render_byte(ctx, 0x05 | (opc << 3));
      else if (get_arg_halfindex(arg1, &opc, &opc2))
        render_2bytes(ctx, 0xDD | (opc << 5), 0x25 | (opc2 << 3));
      else if (get_arg_hl(arg1, &is_ref) && is_ref)
        render_byte(ctx, 0x35);
      else if (get_arg_index_offset8(ctx, arg1, &opc, &opc2, section->curr_pc + 2))
        render_3bytes(ctx, 0xDD | (opc << 5), 0x35, opc2);
      else if (get_arg_qreg16(arg1, &opc2, &is_ref) && !is_ref)
        render_byte(ctx, 0x0B | (opc2 << 4));
      else if (get_arg_index(arg1, &opc, &is_ref) && !is_ref)
        render_2bytes(ctx, 0xDD | (opc << 5), 0x2B);
      else
        ERR_UNEXPECTED_ARGUMENT(1);

      break;
    }

    case DI: {
      render_byte(ctx, 0xF3);
      break;
    }

    case DJNZ: {
      if (get_arg_reladdr(ctx, arg1, &opc, section->curr_pc - section->start + 2))
        render_2bytes(ctx, 0x10, opc);
      else
        ERR_UNEXPECTED_ARGUMENT(1);

      break;
    }

    case EI: {
      render_byte(ctx, 0xFB);
      break;
    }

    case EX: {
      if (get_arg_qreg16(arg1, &opc, &is_ref) && (opc == REG_SP) && is_ref) {
        if (get_arg_hl(arg2, &is_ref) && !is_ref)
          render_byte(ctx, 0xE3);
        else if (get_arg_index(arg2, &opc, &is_ref) && !is_ref)
          render_2bytes(ctx, 0xDD | (opc << 5), 0xE3);
        else
          ERR_UNEXPECTED_ARGUMENT(2);
      } else if (get_arg_af(arg1)) {
        if (get_arg_af_shadow(arg2))
          render_byte(ctx, 0x08);
        else
          ERR_UNEXPECTED_ARGUMENT(2);
      } else if (get_arg_qreg16(arg1, &opc, &is_ref) && (opc == REG_DE) && !is_ref) {
        if (get_arg_hl(arg2, &is_ref) && !is_ref)
          render_byte(ctx, 0xEB);
        else
          ERR_UNEXPECTED_ARGUMENT(2);
      } else {
        ERR_UNEXPECTED_ARGUMENT(1);
      }

      break;
    }

    case EXX: {
      render_byte(ctx, 0xD9);
      break;
    }

    case HALT: {
      render_byte(ctx, 0x76);
      break;
    }

    case IM: {
      if (get_arg_8imm(ctx, arg1, &opc, &is_ref, -1) && (opc == 0) && !is_ref)
        render_2bytes(ctx, 0xED, 0x46);
      else if (get_arg_8imm(ctx, arg1, &opc, &is_ref, -1) && (opc == 1) && !is_ref)
        render_2bytes(ctx, 0xED, 0x56);
      else if (get_arg_8imm(ctx, arg1, &opc, &is_ref, -1) && (opc == 2) && !is_ref)
        render_2bytes(ctx, 0xED, 0x5E);
      else
        ERR_UNEXPECTED_ARGUMENT(1);

      break;
    }

    case IN: {
      if (get_arg_accum(arg1)) {
        if (get_arg_8imm(ctx, arg2, &opc, &is_ref, section->curr_pc - section->start + 2) && is_ref)
          render_2bytes(ctx, 0xDB, opc);
        else
          ERR_UNEXPECTED_ARGUMENT(2);
      } else if (get_arg_gpr8(arg1, &opc, &is_ref) && !is_ref) {
        if (get_arg_gpr8(arg2, &opc2, &is_ref) && is_ref && (opc2 == REG_C))
          render_2bytes(ctx, 0xED, 0x40 | (opc2 << 3));
        else
          ERR_UNEXPECTED_ARGUMENT(2);
      } else if (get_arg_flags(arg1)) {
        if (get_arg_gpr8(arg2, &opc2, &is_ref) && is_ref && (opc2 == REG_C))
          render_2bytes(ctx, 0xED, 0x70);
        else
          ERR_UNEXPECTED_ARGUMENT(2);
      } else {
        ERR_UNEXPECTED_ARGUMENT(1);
      }

      break;
    }

    case INC: {
      if (get_arg_gpr8(arg1, &opc, &is_ref) && !is_ref)
        render_byte(ctx, 0x04 | (opc << 3));
      else if (get_arg_halfindex(arg1, &opc, &opc2))
        render_2bytes(ctx, 0xDD | (opc << 5), 0x24 | (opc2 << 3));
      else if (get_arg_hl(arg1, &is_ref) && is_ref)
        render_byte(ctx, 0x34);
      else if (get_arg_index_offset8(ctx, arg1, &opc, &opc2, section->curr_pc - section->start + 2))
        render_3bytes(ctx, 0xDD | (opc << 5), 0x34, opc2);
      else if (get_arg_qreg16(arg1, &opc2, &is_ref) && !is_ref)
        render_byte(ctx, 0x03 | (opc2 << 4));
      else if (get_arg_index(arg1, &opc, &is_ref) && !is_ref)
        render_2bytes(ctx, 0xDD | (opc << 5), 0x23);
      else
        ERR_UNEXPECTED_ARGUMENT(1);

      break;
    }

    case IND: {
      render_2bytes(ctx, 0xED, 0xAA);
      break;
    }

    case INDR: {
      render_2bytes(ctx, 0xED, 0xBA);
      break;
    }

    case INI: {
      render_2bytes(ctx, 0xED, 0xA2);
      break;
    }

    case INIR: {
      render_2bytes(ctx, 0xED, 0xB2);
      break;
    }

    case JP: {
      if (num_args == 1) {
        int lsb=0, msb=0;

        if (get_arg_16imm(ctx, arg1, &lsb, &msb, &is_ref, section->curr_pc - section->start + 1) && !is_ref)
          render_3bytes(ctx, 0xC3, lsb, msb);
        else if (get_arg_hl(arg1, &is_ref) && is_ref)
          render_byte(ctx, 0xE9);
        else if (get_arg_index(arg1, &lsb, &is_ref) && is_ref)
          render_2bytes(ctx, 0xDD | (lsb << 5), 0xE9);
        else
          ERR_UNEXPECTED_ARGUMENT(1);
      } else if (num_args == 2) {
        int lsb, msb, cond;

        if (get_arg_condition(arg1, &cond)) {
          if (get_arg_16imm(ctx, arg2, &lsb, &msb, &is_ref, section->curr_pc - section->start + 1) && !is_ref)
            render_3bytes(ctx, 0xC2 | (cond << 3), lsb, msb);
          else
            ERR_UNEXPECTED_ARGUMENT(2);
        } else {
          ERR_UNEXPECTED_ARGUMENT(1);
        }
      }

      break;
    }

    case JR: {
      if (num_args == 1) {
        int reladdr;

        if (get_arg_reladdr(ctx, arg1, &reladdr, section->curr_pc - section->start + 1))
          render_2bytes(ctx, 0x18, reladdr);
        else
          ERR_UNEXPECTED_ARGUMENT(1);
      } else if (num_args == 2) {
        int reladdr, cond;

        if (get_arg_reladdr(ctx, arg2, &reladdr, section->curr_pc - section->start + 1)) {
          if (get_arg_condition(arg1, &cond) && (cond == COND_NZ))
            render_2bytes(ctx, 0x20, reladdr);
          else if (get_arg_condition(arg1, &cond) && (cond == COND_Z))
            render_2bytes(ctx, 0x28, reladdr);
          else if (get_arg_condition(arg1, &cond) && (cond == COND_NC))
            render_2bytes(ctx, 0x30, reladdr);
          else if (get_arg_condition(arg1, &cond) && (cond == COND_C))
            render_2bytes(ctx, 0x38, reladdr);
          else
            ERR_UNEXPECTED_ARGUMENT(2);
        } else {
          ERR_UNEXPECTED_ARGUMENT(1);
        }
      }

      break;
    }

    case LD: {
      int i, b, i2, b2, opc3;

      if (get_arg_gpr8(arg1, &opc, &is_ref) && !is_ref) {
        if ((opc == REG_A) && get_arg_qreg16(arg2, &opc2, &is_ref) && is_ref && (opc2 == REG_BC))
          render_byte(ctx, 0x0A);
        else if ((opc == REG_A) && get_arg_qreg16(arg2, &opc2, &is_ref) && is_ref && (opc2 == REG_DE))
          render_byte(ctx, 0x1A);
        else if ((opc == REG_A) && get_arg_16imm(ctx, arg2, &opc3, &opc2, &is_ref, section->curr_pc - section->start + 1) && is_ref)
          render_3bytes(ctx, 0x3A, opc3, opc2);
        else if ((opc == REG_A) && get_arg_intreg(arg2))
          render_2bytes(ctx, 0xED, 0x57);
        else if ((opc == REG_A) && get_arg_rfshreg(arg2))
          render_2bytes(ctx, 0xED, 0x5F);
        else if (get_arg_gpr8(arg2, &opc2, &is_ref) && !is_ref)
          render_byte(ctx, 0x40 | (opc << 3) | opc2);
        else if (get_arg_halfindex(arg2, &i, &b))
          render_2bytes(ctx, 0xDD | (i << 5), 0x44 | (opc << 3) | b);
        else if (get_arg_8imm(ctx, arg2, &opc2, &is_ref, section->curr_pc - section->start + 2) && !is_ref)
          render_2bytes(ctx, 0x06 | (opc << 3), opc2);
        else if (get_arg_hl(arg2, &is_ref) && is_ref)
          render_byte(ctx, 0x46 | (opc << 3));
        else if (get_arg_index_offset8(ctx, arg2, &i, &opc2, section->curr_pc - section->start + 2))
          render_3bytes(ctx, 0xDD | (i << 5), 0x46 | (opc << 3), opc2);
        else if ((opc == REG_A) && (get_arg_16imm(ctx, arg2, &opc, &opc2, &is_ref, section->curr_pc - section->start + 1) && is_ref))
          render_3bytes(ctx, 0x3A, opc, opc2);
        else
          ERR_UNEXPECTED_ARGUMENT(2);
      } else if (get_arg_halfindex(arg1, &i, &b)) {
        if (get_arg_gpr8(arg2, &opc2, &is_ref) && !is_ref)
          render_2bytes(ctx, 0xDD | (i << 5), 0x60 | (b << 3) | opc2);
        else if (get_arg_halfindex(arg2, &i2, &b2)) {
          if ((i == 0) && (b == 0) && (i2 == 0) && (b2 == 1))  // ixh, ixl
            render_2bytes(ctx, 0xDD, 0x65);
          else if ((i == 0) && (b == 1) && (i2 == 0) && (b2 == 0))  // ixl, ixh
            render_2bytes(ctx, 0xDD, 0x6C);
          else if ((i == 1) && (b == 0) && (i2 == 1) && (b2 == 1))  // iyh, iyl
            render_2bytes(ctx, 0xFD, 0x65);
          else if ((i == 1) && (b == 1) && (i2 == 1) && (b2 == 0))  // iyl, iyh
            render_2bytes(ctx, 0xFD, 0x6C);
          else
            ERR_UNEXPECTED_ARGUMENT(2);
        } else {
          ERR_UNEXPECTED_ARGUMENT(2);
        }
      } else if (get_arg_hl(arg1, &is_ref) && is_ref) {
        if (get_arg_gpr8(arg2, &opc, &is_ref) && !is_ref)
          render_byte(ctx, 0x70 | opc);
        else if (get_arg_8imm(ctx, arg2, &opc, &is_ref, section->curr_pc - section->start + 1) && !is_ref)
          render_2bytes(ctx, 0x36, opc);
        else
          ERR_UNEXPECTED_ARGUMENT(2);
      } else if (get_arg_hl(arg1, &is_ref) && !is_ref) {
        if (get_arg_16imm(ctx, arg2, &opc, &opc2, &is_ref, section->curr_pc - section->start + 1) && is_ref)
          render_3bytes(ctx, 0x2A, opc, opc2);
        else if (get_arg_16imm(ctx, arg2, &opc, &opc2, &is_ref, section->curr_pc - section->start + 1) && !is_ref)
          render_3bytes(ctx, 0x01 | (REG_HL << 4), opc, opc2);
        else {
          ERR_UNEXPECTED_ARGUMENT(2);
        }
      } else if (get_arg_index_offset8(ctx, arg1, &i, &opc, section->curr_pc - section->start + 2)) {
        if (get_arg_gpr8(arg2, &opc2, &is_ref) && !is_ref)
          render_3bytes(ctx, 0xDD | (i << 5), 0x70 | opc2, opc);
        else if (get_arg_8imm(ctx, arg2, &opc2, &is_ref, section->curr_pc - section->start + 3) && !is_ref)
          render_4bytes(ctx, 0xDD | (i << 5), 0x36, opc, opc2);
        else
          ERR_UNEXPECTED_ARGUMENT(2);
      } else if (get_arg_qreg16(arg1, &opc, &is_ref) && is_ref && (opc == REG_BC)) {
        if (get_arg_accum(arg2))
          render_byte(ctx, 0x02);
        else
          ERR_UNEXPECTED_ARGUMENT(2);
      } else if (get_arg_qreg16(arg1, &opc, &is_ref) && is_ref && (opc == REG_DE)) {
        if (get_arg_accum(arg2))
          render_byte(ctx, 0x12);
        else
          ERR_UNEXPECTED_ARGUMENT(2);
      } else if (get_arg_16imm(ctx, arg1, &opc, &opc2, &is_ref, -1) && is_ref) {   // TODO!!!
        if (get_arg_accum(arg2))
          render_3bytes(ctx, 0x32, opc, opc2);
        else if (get_arg_qreg16(arg2, &opc3, &is_ref) && !is_ref)
          render_4bytes(ctx, 0xED, 0x43 | (opc3 << 4), opc, opc2);
        else if (get_arg_hl(arg2, &is_ref) && !is_ref)
          render_3bytes(ctx, 0x22, opc, opc2);
        else if (get_arg_index(arg2, &opc3, &is_ref) && !is_ref)
          render_4bytes(ctx, 0xDD | (opc3 << 5), 0x22, opc, opc2);
        else
          ERR_UNEXPECTED_ARGUMENT(2);
      } else if (get_arg_intreg(arg1)) {
        if (get_arg_accum(arg2))
          render_2bytes(ctx, 0xED, 0x47);
        else
          ERR_UNEXPECTED_ARGUMENT(2);
      } else if (get_arg_rfshreg(arg1)) {
        if (get_arg_accum(arg2))
          render_2bytes(ctx, 0xED, 0x4F);
        else
          ERR_UNEXPECTED_ARGUMENT(2);
      } else if (get_arg_qreg16(arg1, &opc, &is_ref) && !is_ref) {
        if ((opc == REG_SP) && get_arg_hl(arg2, &is_ref) && !is_ref)
          render_byte(ctx, 0xF9);
        else if ((opc == REG_SP) && get_arg_index(arg2, &opc2, &is_ref) && !is_ref)
          render_2bytes(ctx, 0xDD | (opc2 << 5), 0xF9);
        else if (get_arg_16imm(ctx, arg2, &opc2, &opc3, &is_ref, section->curr_pc - section->start + 1) && !is_ref)
          render_3bytes(ctx, 0x01 | (opc << 4), opc2, opc3);
        else if (get_arg_16imm(ctx, arg2, &opc2, &opc3, &is_ref, section->curr_pc - section->start + 2) && is_ref)
          render_4bytes(ctx, 0xED, 0x4B | (opc << 4), opc2, opc3);
        else
          ERR_UNEXPECTED_ARGUMENT(2);
      } else if (get_arg_index(arg1, &opc, &is_ref) && !is_ref) {
        if (get_arg_16imm(ctx, arg2, &opc2, &opc3, &is_ref, section->curr_pc - section->start + 2) && !is_ref)
          render_4bytes(ctx, 0xDD | (opc << 5), 0x21, opc2, opc3);
        else if (get_arg_16imm(ctx, arg2, &opc2, &opc3, &is_ref, section->curr_pc - section->start + 2) && is_ref)
          render_4bytes(ctx, 0xDD | (opc << 5), 0x2A, opc2, opc3);
        else
          ERR_UNEXPECTED_ARGUMENT(2);
        ERR_UNEXPECTED_ARGUMENT(1);
      }

      break;
    }

    case LDD: {
      render_2bytes(ctx, 0xED, 0xA8);
      break;
    }

    case LDDR: {
      render_2bytes(ctx, 0xED, 0xB8);
      break;
    }

    case LDI: {
      render_2bytes(ctx, 0xED, 0xA0);
      break;
    }

    case LDIR: {
      render_2bytes(ctx, 0xED, 0xB0);
      break;
    }

    case NEG: {
      render_2bytes(ctx, 0xED, 0x44);
      break;
    }

    case NOP: {
      render_byte(ctx, 0);
      break;
    }

    case OR: {
      if (get_arg_gpr8(arg1, &opc, &is_ref) && !is_ref)
        render_byte(ctx, 0xB0 | opc);
      else if (get_arg_halfindex(arg1, &opc, &opc2))
        render_2bytes(ctx, 0xDD | (opc << 5), 0xB4 | opc2);
      else if (get_arg_8imm(ctx, arg1, &opc, &is_ref, section->curr_pc - section->start + 1) && !is_ref)
        render_2bytes(ctx, 0xF6, opc);
      else if (get_arg_hl(arg1, &is_ref) && is_ref)
        render_byte(ctx, 0xB6);
      else if (get_arg_index_offset8(ctx, arg1, &opc, &opc2, section->curr_pc - section->start + 2))
        render_3bytes(ctx, 0xDD | (opc << 5), 0xB6, opc2);
      else
        ERR_UNEXPECTED_ARGUMENT(1);

      break;
    }

    case OUT: {
      if (get_arg_8imm(ctx, arg1, &opc, &is_ref, section->curr_pc - section->start + 1) && is_ref) {
        if (get_arg_accum(arg2))
          render_2bytes(ctx, 0xD3, opc);
        else
          ERR_UNEXPECTED_ARGUMENT(2);
      } else if (get_arg_gpr8(arg1, &opc, &is_ref) && is_ref && (opc == REG_C)) {
        if (get_arg_gpr8(arg2, &opc2, &is_ref) && !is_ref)
          render_2bytes(ctx, 0xED, 0x41 | (opc2 << 3));
        else if (get_arg_8imm(ctx, arg2, &opc2, &is_ref, -1) && !is_ref && (opc2 == 0))
          render_2bytes(ctx, 0xED, 0x71);
        else
          ERR_UNEXPECTED_ARGUMENT(2);
      } else {
        ERR_UNEXPECTED_ARGUMENT(1);
      }

      break;
    }

    case OUTD: {
      render_2bytes(ctx, 0xED, 0xAB);
      break;
    }

    case OTDR: {
      render_2bytes(ctx, 0xED, 0xBB);
      break;
    }

    case OUTI: {
      render_2bytes(ctx, 0xED, 0xA3);
      break;
    }

    case OTIR: {
      render_2bytes(ctx, 0xED, 0xB3);
      break;
    }

    case POP: {
      if (get_arg_preg16(arg1, &opc, &is_ref) && !is_ref)
        render_byte(ctx, 0xC1 | (opc << 4));
      else if (get_arg_index(arg1, &opc, &is_ref) && !is_ref)
        render_2bytes(ctx, 0xDD | (opc << 5), 0xE1);
      else
        ERR_UNEXPECTED_ARGUMENT(1);

      break;
    }

    case PUSH: {
      if (get_arg_preg16(arg1, &opc, &is_ref) && !is_ref)
        render_byte(ctx, 0xC5 | (opc << 4));
      else if (get_arg_index(arg1, &opc, &is_ref) && !is_ref)
        render_2bytes(ctx, 0xDD | (opc << 5), 0xE5);
      else
        ERR_UNEXPECTED_ARGUMENT(1);

      break;
    }

    case RES: {
      int opc3;

      if (get_arg_bitnum(arg1, &opc)) {
        if (get_arg_gpr8(arg2, &opc2, &is_ref) && !is_ref)
          render_2bytes(ctx, 0xCB, 0x80 | (opc << 3) | opc2);
        else if (get_arg_hl(arg2, &is_ref) && is_ref)
          render_2bytes(ctx, 0xCB, 0x86 | (opc << 3));
        else if (get_arg_index_offset8(ctx, arg2, &opc2, &opc3, section->curr_pc - section->start + 2))
          render_4bytes(ctx, 0xDD | (opc2 << 5), 0xCB, opc3, 0x86 | (opc << 3));
        else
          ERR_UNEXPECTED_ARGUMENT(2);
      } else {
        ERR_UNEXPECTED_ARGUMENT(1);
      }

      break;
    }

    case RET: {
      if (num_args == 0) {
        render_byte(ctx, 0xC9);
      } else if (num_args == 1) {
        if (get_arg_condition(arg1, &opc))
          render_byte(ctx, 0xC0 | (opc << 3));
        else
          ERR_UNEXPECTED_ARGUMENT(1);
      }

      break;
    }

    case RETI: {
      render_2bytes(ctx, 0xED, 0x4D);
      break;
    }

    case RETN: {
      render_2bytes(ctx, 0xED, 0x45);
      break;
    }

    case RLA: {
      render_byte(ctx, 0x17);
      break;
    }

    case RL: {
      if (get_arg_gpr8(arg1, &opc, &is_ref) && !is_ref)
        render_2bytes(ctx, 0xCB, 0x10 | opc);
      else if (get_arg_hl(arg1, &is_ref) && is_ref)
        render_2bytes(ctx, 0xCB, 0x16);
      else if (get_arg_index_offset8(ctx, arg1, &opc, &opc2, section->curr_pc - section->start + 2))
        render_4bytes(ctx, 0xDD | (opc << 5), 0xCB, opc2, 0x16);
      else
        ERR_UNEXPECTED_ARGUMENT(1);

      break;
    }

    case RLCA: {
      render_byte(ctx, 0x07);
      break;
    }

    case RLC: {
      if (get_arg_gpr8(arg1, &opc, &is_ref) && !is_ref)
        render_2bytes(ctx, 0xCB, 0x00 | opc);
      else if (get_arg_hl(arg1, &is_ref) && is_ref)
        render_2bytes(ctx, 0xCB, 0x06);
      else if (get_arg_index_offset8(ctx, arg1, &opc, &opc2, section->curr_pc - section->start + 2))
        render_4bytes(ctx, 0xDD | (opc << 5), 0xCB, opc2, 0x06);
      else
        ERR_UNEXPECTED_ARGUMENT(1);

      break;
    }

    case RLD: {
      render_2bytes(ctx, 0xED, 0x6F);
      break;
    }

    case RRA: {
      render_byte(ctx, 0x1F);
      break;
    }

    case RR: {
      if (get_arg_gpr8(arg1, &opc, &is_ref) && !is_ref)
        render_2bytes(ctx, 0xCB, 0x18 | opc);
      else if (get_arg_hl(arg1, &is_ref) && is_ref)
        render_2bytes(ctx, 0xCB, 0x1E);
      else if (get_arg_index_offset8(ctx, arg1, &opc, &opc2, section->curr_pc - section->start + 2))
        render_4bytes(ctx, 0xDD | (opc << 5), 0xCB, opc2, 0x1E);
      else
        ERR_UNEXPECTED_ARGUMENT(1);

      break;
    }

    case RRCA: {
      render_byte(ctx, 0x0F);
      break;
    }

    case RRC: {
      if (get_arg_gpr8(arg1, &opc, &is_ref) && !is_ref)
        render_2bytes(ctx, 0xCB, 0x08 | opc);
      else if (get_arg_hl(arg1, &is_ref) && is_ref)
        render_2bytes(ctx, 0xCB, 0x0E);
      else if (get_arg_index_offset8(ctx, arg1, &opc, &opc2, section->curr_pc - section->start + 2))
        render_4bytes(ctx, 0xDD | (opc << 5), 0xCB, opc2, 0x0E);
      else
        ERR_UNEXPECTED_ARGUMENT(1);

      break;
    }

    case RRD: {
      render_2bytes(ctx, 0xED, 0x67);
      break;
    }

    case RST: {
      if (get_arg_rstaddr(arg1, &opc))
        render_byte(ctx, 0xC7 | (opc << 3));
      else
        ERR_UNEXPECTED_ARGUMENT(1);

      break;
    }

    case SBC: {
      if (get_arg_accum(arg1)) {
        if (get_arg_gpr8(arg2, &opc, &is_ref) && !is_ref)
          render_byte(ctx, 0x98 | opc);
        else if (get_arg_halfindex(arg2, &opc, &opc2))
          render_2bytes(ctx, 0xDD | (opc << 5), 0x9C | opc2);
        else if (get_arg_8imm(ctx, arg2, &opc, &is_ref, section->curr_pc - section->start + 1) && !is_ref)
          render_2bytes(ctx, 0xDE, opc);
        else if (get_arg_hl(arg2, &is_ref) && is_ref)
          render_byte(ctx, 0x9E);
        else if (get_arg_index_offset8(ctx, arg2, &opc, &opc2, section->curr_pc - section->start + 2))
          render_3bytes(ctx, 0xDD | (opc << 5), 0x9E, opc2);
        else
          ERR_UNEXPECTED_ARGUMENT(2);
      } else if (get_arg_hl(arg1, &is_ref) && !is_ref) {
        if (get_arg_qreg16(arg2, &opc, &is_ref) && !is_ref)
          render_2bytes(ctx, 0xED, 0x42 | (opc << 4));
        else
          ERR_UNEXPECTED_ARGUMENT(2);
      } else {
        ERR_UNEXPECTED_ARGUMENT(1);
      }

      break;
    }

    case SCF: {
      render_byte(ctx, 0x37);
      break;
    }

    case SET: {
      int opc3;

      if (get_arg_bitnum(arg1, &opc)) {
        if (get_arg_gpr8(arg2, &opc2, &is_ref) && !is_ref)
          render_2bytes(ctx, 0xCB, 0xC0 | (opc << 3) | opc2);
        else if (get_arg_hl(arg2, &is_ref) && is_ref)
          render_2bytes(ctx, 0xCB, 0xC6 | (opc << 3));
        else if (get_arg_index_offset8(ctx, arg2, &opc2, &opc3, section->curr_pc - section->start + 2))
          render_4bytes(ctx, 0xDD | (opc2 << 5), 0xCB, opc3, 0xC6 | (opc << 3));
        else
          ERR_UNEXPECTED_ARGUMENT(2);
      } else {
        ERR_UNEXPECTED_ARGUMENT(1);
      }

      break;
    }

    case SLA: {
      if (get_arg_gpr8(arg1, &opc, &is_ref) && !is_ref)
        render_2bytes(ctx, 0xCB, 0x20 | opc);
      else if (get_arg_hl(arg1, &is_ref) && is_ref)
        render_2bytes(ctx, 0xCB, 0x26);
      else if (get_arg_index_offset8(ctx, arg1, &opc, &opc2, section->curr_pc - section->start + 2))
        render_4bytes(ctx, 0xDD | (opc << 5), 0xCB, opc2, 0x26);
      else
        ERR_UNEXPECTED_ARGUMENT(1);

      break;
    }

    case SRA: {
      if (get_arg_gpr8(arg1, &opc, &is_ref) && !is_ref)
        render_2bytes(ctx, 0xCB, 0x28 | opc);
      else if (get_arg_hl(arg1, &is_ref) && is_ref)
        render_2bytes(ctx, 0xCB, 0x2E);
      else if (get_arg_index_offset8(ctx, arg1, &opc, &opc2, section->curr_pc - section->start + 2))
        render_4bytes(ctx, 0xDD | (opc << 5), 0xCB, opc2, 0x2E);
      else
        ERR_UNEXPECTED_ARGUMENT(1);

      break;
    }

    case SLL: {
      if (get_arg_gpr8(arg1, &opc, &is_ref) && !is_ref)
        render_2bytes(ctx, 0xCB, 0x30 | opc);
      else if (get_arg_hl(arg1, &is_ref) && is_ref)
        render_2bytes(ctx, 0xCB, 0x36);
      else if (get_arg_index_offset8(ctx, arg1, &opc, &opc2, section->curr_pc - section->start + 2))
        render_4bytes(ctx, 0xDD | (opc << 5), 0xCB, opc2, 0x36);
      else
        ERR_UNEXPECTED_ARGUMENT(1);

      break;
    }

    case SRL: {
      if (get_arg_gpr8(arg1, &opc, &is_ref) && !is_ref)
        render_2bytes(ctx, 0xCB, 0x38 | opc);
      else if (get_arg_hl(arg1, &is_ref) && is_ref)
        render_2bytes(ctx, 0xCB, 0x3E);
      else if (get_arg_index_offset8(ctx, arg1, &opc, &opc2, section->curr_pc - section->start + 2))
        render_4bytes(ctx, 0xDD | (opc << 5), 0xCB, opc2, 0x3E);
      else
        ERR_UNEXPECTED_ARGUMENT(1);

      break;
    }

    case SUB: {
      if (get_arg_gpr8(arg1, &opc, &is_ref) && !is_ref)
        render_byte(ctx, 0x90 | opc);
      else if (get_arg_halfindex(arg1, &opc, &opc2))
        render_2bytes(ctx, 0xDD | (opc << 5), 0x94 | opc2);
      else if (get_arg_8imm(ctx, arg1, &opc, &is_ref, section->curr_pc - section->start + 1) && !is_ref)
        render_2bytes(ctx, 0xD6, opc);
      else if (get_arg_hl(arg1, &is_ref) && is_ref)
        render_byte(ctx, 0x96);
      else if (get_arg_index_offset8(ctx, arg1, &opc, &opc2, section->curr_pc - section->start + 2))
        render_3bytes(ctx, 0xDD | (opc << 5), 0x96, opc2);
      else
        ERR_UNEXPECTED_ARGUMENT(1);

      break;
    }

    case XOR: {
      if (get_arg_gpr8(arg1, &opc, &is_ref) && !is_ref)
        render_byte(ctx, 0xA8 | opc);
      else if (get_arg_halfindex(arg1, &opc, &opc2))
        render_2bytes(ctx, 0xDD | (opc << 5), 0xAC | opc2);
      else if (get_arg_8imm(ctx, arg1, &opc, &is_ref, section->curr_pc - section->start + 1) && !is_ref)
        render_2bytes(ctx, 0xEE, opc);
      else if (get_arg_hl(arg1, &is_ref) && is_ref)
        render_byte(ctx, 0xAE);
      else if (get_arg_index_offset8(ctx, arg1, &opc, &opc2, section->curr_pc - section->start + 2))
        render_3bytes(ctx, 0xDD | (opc << 5), 0xAE, opc2);
      else
        ERR_UNEXPECTED_ARGUMENT(1);

      break;
    }

    default:
      // should never reach here
      assert(0);
      break;
  }
}

#include <stdio.h>
#include <string.h>
#include <unistd.h>
#include <ctype.h>
#include <stdarg.h>

#include "filesystem.h"
#include "disas.h"

#define INSTR_COLUMN 8
#define COMMENT_COLUMN 32
#define ASCII_DUMP_OFFSET  24

// convert 8/16 bit integer to hexademical representation
// according 'Intel assembler' format: with h-suffix and leading
// '0' character for numbers with first letter hex char
static char *int_to_hex(uint16_t value, bool expl16bit) {
  char tmp[16];
  char *fmt, *res, *p;
  if ((value & 0xff00) || expl16bit)
    fmt = "%04xh";
  else
    fmt = "%02xh";

  int len = snprintf(tmp, sizeof(tmp), fmt, value);
  res = xmalloc(len + 2);
  p = res;

  if (tmp[0] >= 'a' && tmp[0] <= 'f') {
    res[0] = '0';
    p++;
  }
  strcpy(p, tmp);
  return res;
}

static char *str_tolower(char *str) {
  char *res = xstrdup(str);
  char *p = res;
  while (*p) {
    *p = tolower(*p);
    p++;
  }
  return res;
}

static char *get_label_name(disas_context_t *ctx, uint16_t addr) {
  char tmp[16];
  int id = 0;

  if (!HAS_LABEL(ctx, addr))
    return NULL;

  for (int i = 0; i <= addr; i++)
    if (HAS_LABEL(ctx, i))
      id++;

  snprintf(tmp, sizeof(tmp), "lbl_%d", id);
  return xstrdup(tmp);
}

static void disas_enlarge(disas_context_t *ctx, int len)
{
  int newlen;

  len += ctx->out_len + 1;

  if (len <= ctx->out_capacity)
    return;

  newlen = 2 * ctx->out_capacity;
  while (len > newlen)
    newlen = 2 * newlen;

  ctx->out_str = (char *) xrealloc(ctx->out_str, newlen);

  ctx->out_capacity = newlen;
}

static int disas_append_va(disas_context_t *ctx, const char *fmt, va_list args)
{
  int avail;
  size_t nprinted;

  avail = ctx->out_capacity - ctx->out_len;
  nprinted = vsnprintf(ctx->out_str + ctx->out_len, (size_t)avail, fmt, args);

  if (nprinted < (size_t) avail) {
    ctx->out_len += (int)nprinted;
    return 0;
  }

  ctx->out_str[ctx->out_len] = '\0';

  return (int)nprinted;
}

static int disas_printf(disas_context_t *ctx, char *fmt, ...) {
  int prev_len = ctx->out_len;

  for (;;) {
    va_list args;
    int needed;

    va_start(args, fmt);
    needed = disas_append_va(ctx, fmt, args);
    va_end(args);

    if (needed == 0)
      break;

    disas_enlarge(ctx, needed);
  }

  return ctx->out_len - prev_len;
}

void disas_render_text(disas_context_t *ctx) {
  for (int i = 0; i < INSTR_COLUMN; i++) {
    disas_printf(ctx, " ");
  }

  char *orgval = int_to_hex(ctx->org, true);
  disas_printf(ctx, "org %s\n\n", orgval);
  xfree(orgval);

  for (disas_node_t *node = ctx->nodes; node != NULL; node = node->next) {
    int column = 0;

    if (ctx->opt_labels) {
      char *label_name = get_label_name(ctx, node->addr);
      if (label_name) {
        disas_printf(ctx, "%s:\n", label_name);
        xfree(label_name);
      }
    }

    for (int i = 0; i < INSTR_COLUMN; i++) {
      disas_printf(ctx, " ");
      column++;
    }

    if (node->valid == false) {
      char *defval = int_to_hex(node->instr, false);
      column += disas_printf(ctx, "defb %s", defval);
      xfree(defval);

      for (int i = 0; i < (COMMENT_COLUMN - column); i++)
        disas_printf(ctx, " ");
      column = COMMENT_COLUMN;

      disas_printf(ctx, "; %04x  invalid opcode %02xh\n", node->addr, node->instr);
      continue;
    }

    char *mnemonic = str_tolower(MnemonicStrings[node->instr]);
    column += disas_printf(ctx, "%s ", mnemonic);
    xfree(mnemonic);

    assert(node->num_args <= 2);

    for (int i = 0; i < node->num_args; i++) {
      switch (node->args[i].kind) {
        case ARG_16IMM: {
          uint16_t ival16 = ((uint16_t)node->args[i].extra << 8) | (uint16_t)node->args[i].value;
          char *strval = NULL;
          if (ctx->opt_labels && (node->instr == JP || node->instr == CALL))
            strval = get_label_name(ctx, ival16);

          if (strval == NULL)
            strval = int_to_hex(ival16, true);

          column += disas_printf(ctx, "%s%s%s",
            (node->args[i].is_ref ? "(" : ""),
            strval,
            (node->args[i].is_ref ? ")" : ""));

          xfree(strval);
          break;
        }

        case ARG_INDEX: {
          int8_t off8 = (int8_t)(node->args[i].extra & 0xff);
          column += disas_printf(ctx, "(%s%s%d)",
            (node->args[i].value == 0) ? "ix" : "iy",
            (off8 >= 0) ? "+" : "-",
            (off8 >= 0) ? off8 : -off8);
          break;
        }

        case ARG_8IMM: {
          char *strval = int_to_hex(node->args[i].value, false);
          column += disas_printf(ctx, "%s%s%s",
            (node->args[i].is_ref ? "(" : ""),
            strval,
            (node->args[i].is_ref ? ")" : ""));
          xfree(strval);
          break;
        }

        case ARG_BITNUM:
          column += disas_printf(ctx, "%d", node->args[i].value);
          break;

        case ARG_8GPR: {
          static char *regnames[] = {"b", "c", "d", "e", "h", "l", "<?>", "a"};
          column += disas_printf(ctx, "%s%s%s",
            (node->args[i].is_ref ? "(" : ""),
            regnames[node->args[i].value % 8],
            (node->args[i].is_ref ? ")" : ""));
          break;
        }

        case ARG_INTERRUPT:
          disas_printf(ctx, "i");
          column += 1;
          break;

        case ARG_REFRESH:
          disas_printf(ctx, "r");
          column += 1;
          break;

        case ARG_REGPAIR_P: {
          static char *regnames_16p[] = {"bc", "de", "hl", "af"};
          column += disas_printf(ctx, "%s%s%s",
            (node->args[i].is_ref ? "(" : ""),
            regnames_16p[node->args[i].value % 4],
            (node->args[i].is_ref ? ")" : ""));
          break;
        }

        case ARG_REGPAIR_I: {
          static char *regnames_16i[] = {"ix", "iy"};
          column += disas_printf(ctx, "%s%s%s",
            (node->args[i].is_ref ? "(" : ""),
            regnames_16i[node->args[i].value % 2],
            (node->args[i].is_ref ? ")" : ""));
          break;
        }

        case ARG_REGPAIR_Q: {
          static char *regnames_16q[] = {"bc", "de", "hl", "sp"};
          column += disas_printf(ctx, "%s%s%s",
            (node->args[i].is_ref ? "(" : ""),
            regnames_16q[node->args[i].value % 4],
            (node->args[i].is_ref ? ")" : ""));
          break;
        }

        case ARG_AF_SHADOW:
          disas_printf(ctx, "af'");
          column += 3;
          break;

        case ARG_CONDITION: {
          static char *condnames[] = {"nz", "z", "nc", "c", "po", "pe", "p", "m"};
          column += disas_printf(ctx, "%s", condnames[node->args[i].value % 8]);
          break;
        }

        case ARG_RELJUMP: {
          int8_t next_instr_offset = (int8_t)(node->args[i].value & 0xff);

          // add current JR instruction size
          next_instr_offset += node->args[i].extra;

          column += disas_printf(ctx, "$%s%d",
            (next_instr_offset >= 0) ? "+" : "-",
            (next_instr_offset >= 0) ? next_instr_offset : -next_instr_offset);

          break;
        }

        case ARG_RST: {
          uint8_t rstval[] = {0, 8, 0x10, 0x18, 0x20, 0x28, 0x30, 0x38};
          char *strval = int_to_hex(rstval[node->args[i].value % 8], false);
          column += disas_printf(ctx, "%s", strval);
          xfree(strval);
          break;
        }

        default:
          disas_printf(ctx, "<?>");
          column += 3;
          break;
      }

      if (i < node->num_args - 1) {
        disas_printf(ctx, ", ");
        column += 2;
      }
    } // for (args)

    if (ctx->opt_addr || ctx->opt_source) {
      // add comment at the line end
      for (int i = 0; i < (COMMENT_COLUMN - column); i++)
        disas_printf(ctx, " ");
      column = COMMENT_COLUMN;

      disas_printf(ctx, "; ");
      column += 2;

      if (ctx->opt_addr)
        column += disas_printf(ctx, "%04x", node->addr);

      if (ctx->opt_source) {
        // hex opcode
        disas_printf(ctx, "   ");
        column += 3;
        for (int i = 0; i < node->isize; i++)
          column += disas_printf(ctx, "%02x ", ctx->binary[node->addr + i]);

        // opcode ASCII dump
        for (int i = 0; i < (COMMENT_COLUMN + ASCII_DUMP_OFFSET - column); i++)
          disas_printf(ctx, " ");
        column = COMMENT_COLUMN + ASCII_DUMP_OFFSET;

        for (int i = 0; i < node->isize; i++)
          column += disas_printf(ctx, "%c",
            isprint(ctx->binary[node->addr + i]) ? ctx->binary[node->addr + i] : '.');
      }
    }

    disas_printf(ctx, "\n");

    // extra line for subroutine end
    if ((node->instr == RET && (node->num_args == 0)) || node->instr == RETI || node->instr == RETN)
      disas_printf(ctx, "\n");
  } // for (nodes)
}

#include <stdint.h>
#include <stdio.h>
#include <string.h>

#include "asm/compile.h"
#include "asm/render.h"
#include "common/buffer.h"
#include "common/hashmap.h"

static char *reserved_ids[] = {
  "A", "B", "C", "D", "E", "H", "L", "F", "I", "R", "BC", "DE", "HL", "AF", "AF'", "SP",
  "IX", "IY", "IXH", "IXL", "IYH", "IYL", "NZ", "Z", "NC", "C", "PO", "PE", "P", "M",

  "ADC", "ADD", "AND", "BIT", "CALL", "CCF", "CP", "CPD", "CPDR", "CPI", "CPIR",
  "CPL", "DAA", "DEC", "DI", "DJNZ", "EI", "EX", "EXX", "HALT", "IM", "IN", "INC",
  "IND", "INDR", "INI", "INIR", "JP", "JR", "LD", "LDD", "LDDR", "LDI", "LDIR",
  "NEG", "NOP", "OR", "OTDR", "OTIR", "OUT", "OUTD", "OUTI", "POP", "PUSH",
  "RES", "RET", "RETI", "RETN", "RL", "RLA", "RLC", "RLCA", "RLD", "RR", "RRA",
  "RRCA", "RRD", "RST", "SBC", "SCF", "SET", "SLA", "SRA", "SRL", "SUB", "XOR",

  "ORG", "REPT", "ENDR", "PROFILE", "ENDPROFILE", "EQU", "END", "DB", "DW", "DS",
  "DM", "DEFB", "DEFW", "DEFS", "DEFM", "INCBIN", "INCLUDE", "SECTION",
  "IF", "ELSE", "ENDIF",
  NULL};

bool is_reserved_ident(const char *id)
{
  for (int i = 0;; i++) {
    char *rsvname = reserved_ids[i];
    if (rsvname == NULL)
      break;

    if (strcasecmp(id, rsvname) == 0)
      return true;
  }

  return false;
}

hashmap *make_symtab(hashmap *defineopts)
{
  hashmap_scan *scan = NULL;
  hashmap_entry *entry = NULL;
  hashmap *symtab = hashmap_create(1024, "symtab");

  if (!defineopts)
    return symtab;

  scan = hashmap_scan_init(defineopts);
  while ((entry = hashmap_scan_next(scan)) != NULL) {
    LITERAL *l = make_node_internal(LITERAL);

    if (parse_any_integer(entry->value, &l->ival)) {
      l->kind = INT;
    } else {
      l->kind = STR;
      l->strval = xstrdup(entry->value);
    }

    hashmap_search(symtab, entry->key, HASHMAP_INSERT, l);
  }

  return symtab;
}

parse_node *add_sym_variable_node(compile_ctx_t *ctx, const char *name, parse_node *value)
{
  if (is_reserved_ident(name))
    report_error(ctx, "can't redefine reserved identifier %s", name);

  return hashmap_search(ctx->symtab, (void *)name, HASHMAP_INSERT, value);
}

parse_node *add_sym_variable_integer(compile_ctx_t *ctx, const char *name, int ival)
{
  LITERAL *l = make_node_internal(LITERAL);
  l->kind = INT;
  l->is_ref = false;
  l->ival = ival;

  return add_sym_variable_node(ctx, name, (parse_node *)l);
}

parse_node *get_sym_variable(compile_ctx_t *ctx, const char *name, bool missing_ok)
{
  parse_node *node = hashmap_search(ctx->symtab, (void *)name, HASHMAP_FIND, NULL);
  if (node == NULL) {
    if (missing_ok)
      return NULL;

    report_error(ctx, "variable %s is not defined", name);
  }

  assert(node->type == NODE_LITERAL);

  return node;
}

parse_node *remove_sym_variable(compile_ctx_t *ctx, const char *name)
{
  return (parse_node *)hashmap_search(ctx->symtab, (void *)name, HASHMAP_REMOVE, NULL);
}

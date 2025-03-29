#pragma once

#include <setjmp.h>
#include <stdbool.h>
#include <stdint.h>

#include "bits/mmgr.h"

typedef struct dynarray dynarray;

typedef enum parse_type
{
  NODE_DEF = 0,  // db, defb, dm, defm, dw, defw
  NODE_END,
  NODE_ORG,
  NODE_INCBIN,
  NODE_EQU,
  NODE_LITERAL,
  NODE_LABEL,
  NODE_INSTR,
  NODE_EXPR,
  NODE_ID,
  NODE_LIST,
  NODE_SECTION,
  NODE_REPT,
  NODE_ENDR,
  NODE_PROFILE,
  NODE_ENDPROFILE,
  NODE_IF,
  NODE_ELSE,
  NODE_ENDIF,
} parse_type;

typedef struct parse_node {
  parse_type type;
  uint32_t line;
  uint32_t pos;
  char *fn;
  bool is_ref;
  char data[0];
} parse_node;

typedef struct {
  parse_node hdr;
  char *name;
} ID;

typedef enum
{
  DEFKIND_DB = 0,
  DEFKIND_DM,
  DEFKIND_DW,
  DEFKIND_DS
} defkind;

typedef struct {
  parse_node hdr;
  dynarray *list;
} LIST;

typedef struct {
  parse_node hdr;
  defkind kind;
  LIST *values;
} DEF;

typedef struct {
  parse_node hdr;
} END;

typedef enum
{
  INT = 0,
  STR,
  DOLLAR
} litkind;

typedef struct {
  parse_node hdr;
  litkind kind;
  char *strval;
  int ival;
} LITERAL;

typedef enum
{
  SIMPLE = 0,
  UNARY_PLUS,
  UNARY_MINUS,
  UNARY_INV,
  UNARY_NOT,
  BINARY_PLUS,
  BINARY_MINUS,
  BINARY_MUL,
  BINARY_DIV,
  BINARY_AND,
  BINARY_OR,
  BINARY_MOD,
  BINARY_SHL,
  BINARY_SHR,
  COND_EQ,
  COND_NE,
  COND_LT,
  COND_LE,
  COND_GT,
  COND_GE,
} exprkind;

typedef struct {
  parse_node hdr;
  exprkind kind;
  parse_node *left;
  parse_node *right;
} EXPR;

typedef struct {
  parse_node hdr;
  parse_node *value;
} ORG;

typedef struct {
  parse_node hdr;
  LITERAL *filename;
} INCBIN;

typedef struct {
  parse_node hdr;
  ID *name;
  EXPR *value;
} EQU;

typedef struct {
  parse_node hdr;
  EXPR *count_expr;
  ID *var;
} REPT;

typedef struct {
  parse_node hdr;
} ENDR;

typedef struct {
  parse_node hdr;
  ID *name;
} LABEL;

typedef struct {
  parse_node hdr;
  ID *name;
  LIST *args; // array of expressions
} INSTR;

typedef struct {
  parse_node hdr;
  LITERAL *name;
  LIST *params;
} SECTION;

typedef struct {
  parse_node hdr;
  LITERAL *name;
} PROFILE;

typedef struct {
  parse_node hdr;
} ENDPROFILE;

typedef struct {
  parse_node hdr;
  EXPR *condition;
} IF;

typedef struct {
  parse_node hdr;
} ELSE;

typedef struct {
  parse_node hdr;
} ENDIF;

extern parse_node *new_node_macro_holder;

#define new_node(size, t, fn_, line_, pos_) \
( \
  new_node_macro_holder = (parse_node *)xmalloc2(size, #t), \
  new_node_macro_holder->type = (t),                        \
  new_node_macro_holder->line = (line_)+1,                  \
  new_node_macro_holder->pos = (pos_),                      \
  new_node_macro_holder->fn = (fn_),                        \
  new_node_macro_holder \
)

#define make_node(t, fn, line_, pos_)    ((t *) new_node(sizeof(t), NODE_##t, (fn), (line_), (pos_)))
#define make_node_internal(t)    ((t *) new_node(sizeof(t), NODE_##t, NULL, 0, 0))

typedef struct dynarray dynarray;
typedef struct hashmap hashmap;

struct as_scanner_state {
  int line_num;
  int pos_num;
};

#define IS_INT_LITERAL(node) \
    ((((parse_node *)node)->type == NODE_LITERAL) && (((LITERAL *)node)->kind == INT))

struct bc80asm_args;

extern int parse_source(char *filename, char *source, dynarray **statements);
extern int parse_include(char *filename, dynarray **statements);

extern int parse_decnum(char *text, int len);
extern int parse_hexnum(char *text, int len);
extern int parse_binnum(char *text, int len);

extern void print_node(parse_node *node);
extern char *node_to_string(parse_node *node);
extern void dump_parse_list(dynarray *statements);
extern const char *get_parse_node_name(parse_node *node);
extern const char *get_literal_kind(LITERAL *l);

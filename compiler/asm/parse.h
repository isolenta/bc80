#pragma once

#include <stdbool.h>
#include <setjmp.h>

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
  NODE_ENDR
} parse_type;

typedef struct {
  parse_type type;
  uint32_t line;
  char *fn;
  char data[0];
} parse_node;

typedef struct {
  parse_type type;
  uint32_t line;
  const char *fn;
  bool is_ref;
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
  parse_type type;
  uint32_t line;
  const char *fn;
  dynarray *list;
} LIST;

typedef struct {
  parse_type type;
  uint32_t line;
  const char *fn;
  defkind kind;
  LIST *values;
} DEF;

typedef struct {
  parse_type type;
  uint32_t line;
  const char *fn;
} END;

typedef enum
{
  INT = 0,
  STR,
  DOLLAR
} litkind;

typedef struct {
  parse_type type;
  uint32_t line;
  const char *fn;
  litkind kind;
  bool is_ref;
  char *strval;
  int ival;
} LITERAL;

typedef enum
{
  SIMPLE = 0,
  UNARY_PLUS,
  UNARY_MINUS,
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
} exprkind;

typedef struct {
  parse_type type;
  uint32_t line;
  const char *fn;
  exprkind kind;
  bool is_ref;
  parse_node *left;
  parse_node *right;
} EXPR;

typedef struct {
  parse_type type;
  uint32_t line;
  const char *fn;
  parse_node *value;
} ORG;

typedef struct {
  parse_type type;
  uint32_t line;
  const char *fn;
  LITERAL *filename;
} INCBIN;

typedef struct {
  parse_type type;
  uint32_t line;
  const char *fn;
  ID *name;
  EXPR *value;
} EQU;

typedef struct {
  parse_type type;
  uint32_t line;
  const char *fn;
  LITERAL *count;
} REPT;

typedef struct {
  parse_type type;
  uint32_t line;
  const char *fn;
} ENDR;

typedef struct {
  parse_type type;
  uint32_t line;
  const char *fn;
  ID *name;
} LABEL;

typedef struct {
  parse_type type;
  uint32_t line;
  const char *fn;
  ID *name;
  LIST *args; // array of expressions
} INSTR;

typedef struct {
  parse_type type;
  uint32_t line;
  const char *fn;
  LIST *args;
} SECTION;

extern parse_node *new_node_macro_holder;

#define new_node(size, t, fn_, loc) \
( \
  new_node_macro_holder = (parse_node *)xmalloc2(size, #t), \
  new_node_macro_holder->type = (t), \
  new_node_macro_holder->line = (loc)+1, \
  new_node_macro_holder->fn = (fn_), \
  new_node_macro_holder \
)

#define make_node(t, fn, loc)    ((t *) new_node(sizeof(t), NODE_##t, (fn), (loc)))

typedef struct dynarray dynarray;
typedef struct hashmap hashmap;
struct libasm_as_desc_t;

extern void print_node(parse_node *node);
extern char *node_to_string(parse_node *node);
extern int parse_integer(struct libasm_as_desc_t *desc, char *text, int len, int base, char suffix, jmp_buf *parse_env);
extern int parse_binary(struct libasm_as_desc_t *desc, char *text, int len, jmp_buf *parse_env);
extern int parse_include(struct libasm_as_desc_t *desc, dynarray **statements, char *filename, int line, jmp_buf *parse_env);
extern void parse_print(dynarray *statements);
extern int parse_string(struct libasm_as_desc_t *desc, dynarray **statements, jmp_buf *parse_env);
extern const char *get_parse_node_name(parse_node *node);
extern const char *get_literal_kind(LITERAL *l);

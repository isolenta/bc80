#pragma once

#include <string.h>

#include "common/mmgr.h"
#include "rcc/parser.h"
#include "rcc/nodes.h"

typedef enum LiteralKind
{
  LITINT = 0,
  LITSTR,
  LITBOOL,
} LiteralKind;

typedef enum SpecifierKind {
  SPEC_STATIC = 0,
  SPEC_INLINE,
  SPEC_VOID,
  SPEC_INT8,
  SPEC_UINT8,
  SPEC_INT16,
  SPEC_UINT16,
  SPEC_BOOL,
  SPEC_STRUCT,
  SPEC_ALIGN,
  SPEC_SECTION,
} SpecifierKind;

typedef enum DesignatorKind {
  DESIGNATOR_ARRAY = 0,
  DESIGNATOR_FIELD,
} DesignatorKind;

typedef enum SelectStmtKind {
  SELECT_IF = 0,
  SELECT_SWITCH,
} SelectStmtKind;

typedef enum IterationStmtKind {
  ITERATION_WHILE = 0,
  ITERATION_DO,
  ITERATION_FOR,
} IterationStmtKind;

typedef enum JumpStmtKind {
  JUMP_GOTO = 0,
  JUMP_CONTINUE,
  JUMP_BREAK,
  JUMP_RETURN,
} JumpStmtKind;

typedef enum LabeledStmtKind {
  LABELED_LABEL = 0,
  LABELED_CASE,
  LABELED_DEFAULT,
} LabeledStmtKind;

typedef struct IDENTIFIER IDENTIFIER;
typedef struct LITERAL LITERAL;

typedef struct STATIC_ASSERT_DECL {
  Node hdr;
  EXPRESSION *expr;
  LITERAL *message;
} STATIC_ASSERT_DECL;

typedef struct LITERAL {
  Node hdr;
  LiteralKind kind;
  int ival;
  char *strval;
  bool bval;
} LITERAL;

struct IDENTIFIER {
  Node hdr;
  char *name;
};

typedef struct POSITION_DECL {
  Node hdr;
  char *file;
  int line;
} POSITION_DECL;

typedef struct LABELED_STMT {
  Node hdr;
  LabeledStmtKind kind;
  Node *name;
} LABELED_STMT;

typedef struct SPECIFIER {
  Node hdr;
  SpecifierKind kind;
  IDENTIFIER *name;    // for structs
  Node *arg;           // for parametrized specifiers (align, section)
} SPECIFIER;

typedef struct DESIGNATOR {
  Node hdr;
  DesignatorKind kind;
} DESIGNATOR;

typedef struct SELECT_STMT {
  Node hdr;
  SelectStmtKind kind;
  Node *expr;
} SELECT_STMT;

typedef struct ITERATION_STMT {
  Node hdr;
  IterationStmtKind kind;
  Node *init;
  Node *cond;
  Node *step;
} ITERATION_STMT;

typedef struct JUMP_STMT {
  Node hdr;
  JumpStmtKind kind;
} JUMP_STMT;

typedef struct FUNCTION_DEF {
  Node hdr;
  LIST *specifiers;
  Node *declarator;
  LIST *statements;
} FUNCTION_DEF;

typedef struct CAST {
  Node hdr;
  Node *cast_to;
} CAST;

static inline const char *parse_node_specifier_kind(SpecifierKind kind) {
  switch(kind) {
    case SPEC_STATIC:
      return "static";
    case SPEC_INLINE:
      return "inline";
    case SPEC_VOID:
      return "void";
    case SPEC_INT8:
      return "i8";
    case SPEC_UINT8:
      return "u8";
    case SPEC_INT16:
      return "i16";
    case SPEC_UINT16:
      return "u16";
    case SPEC_BOOL:
      return "bool";
    case SPEC_STRUCT:
      return "struct";
    case SPEC_ALIGN:
      return "align";
    case SPEC_SECTION:
      return "section";
  }
  return "???";
}

static inline const char *parse_node_jump_kind(JumpStmtKind kind) {
  switch(kind) {
    case JUMP_GOTO:
      return "goto";
    case JUMP_CONTINUE:
      return "continue";
    case JUMP_BREAK:
      return "break";
    case JUMP_RETURN:
      return "return";
  }
  return "???";
}

static inline const char *parse_node_select_kind(SelectStmtKind kind) {
  switch(kind) {
    case SELECT_IF:
      return "if";
    case SELECT_SWITCH:
      return "switch";
  }
  return "???";
}

static inline const char *parse_node_labeled_kind(LabeledStmtKind kind) {
  switch(kind) {
    case LABELED_LABEL:
      return "label";
    case LABELED_CASE:
      return "case";
    case LABELED_DEFAULT:
      return "default";
  }
  return "???";
}

static inline const char *parse_node_iteration_kind(IterationStmtKind kind) {
  switch(kind) {
    case ITERATION_WHILE:
      return "while";
    case ITERATION_DO:
      return "do";
    case ITERATION_FOR:
      return "for";
  }
  return "???";
}

static inline const char *parse_node_designator_kind(DesignatorKind kind) {
  switch(kind) {
    case DESIGNATOR_ARRAY:
      return "array";
    case DESIGNATOR_FIELD:
      return "field";
  }
  return "???";
}

extern Node *rc_node_macro_holder;

#define CreateParseNode(t, first_line)                                   \
(                                                                        \
  rc_node_macro_holder = (Node *)xmalloc2(sizeof(t), #t),                \
  memset(rc_node_macro_holder, 0, sizeof(t)),                            \
  rc_node_macro_holder->type = (T_##t),                                  \
  rc_node_macro_holder->childs = (LIST *)xmalloc2(sizeof(LIST), "LIST"), \
  memset(rc_node_macro_holder->childs, 0, sizeof(LIST)),                 \
  rc_node_macro_holder->childs->hdr.type = T_LIST,                       \
  rc_node_macro_holder->line = first_line,                               \
  rc_node_macro_holder->pos = ctx->current_position.pos,                 \
  rc_node_macro_holder->file = xstrdup(ctx->current_position.filename),  \
  (t *)rc_node_macro_holder                                              \
)

#define CreatePrimitiveParseNode(t, first_line)                          \
(                                                                        \
  rc_node_macro_holder = (Node *)xmalloc(sizeof(Node)),                  \
  memset(rc_node_macro_holder, 0, sizeof(Node)),                         \
  rc_node_macro_holder->type = (T_##t),                                  \
  rc_node_macro_holder->childs = (LIST *)xmalloc2(sizeof(LIST), "LIST"), \
  memset(rc_node_macro_holder->childs, 0, sizeof(LIST)),                 \
  rc_node_macro_holder->childs->hdr.type = T_LIST,                       \
  rc_node_macro_holder->line = first_line,                               \
  rc_node_macro_holder->pos = ctx->current_position.pos,                 \
  rc_node_macro_holder->file = xstrdup(ctx->current_position.filename),  \
  rc_node_macro_holder                                                   \
)

#define AddChild(_parent, _child)                                              \
  if (_child) {                                                                \
    ((Node *)_parent)->childs->list =                                          \
    dynarray_append_ptr(((Node *)_parent)->childs->list, _child);              \
    ((Node *)_child)->parent = (Node *)_parent;                                \
  }

typedef bool(*parse_tree_walker_fn)(Node *, void *context);
extern bool parse_tree_walker(Node *root, parse_tree_walker_fn walker, void *context);

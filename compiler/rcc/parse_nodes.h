#pragma once

#include <string.h>

#include "common/dynarray.h"
#include "common/mmgr.h"
#include "rcc/parser.h"

typedef enum NodeType
{
  T_Node,
  T_ARG_EXPR_LIST,
  T_ASSERT_DECL,
  T_BLOCK_LIST,
  T_CAST,
  T_DECL_LIST,
  T_DECL_SPEC_LIST,
  T_DESIGNATOR,
  T_DESIGNATOR_LIST,
  T_EXPR_LIST,
  T_EXPRESSION,
  T_FUNCTION_DEF,
  T_IDENTIFIER,
  T_IDENTIFIER_LIST,
  T_INIT_DECLARATOR_LIST,
  T_INITIALIZER_LIST,
  T_ITERATION_STMT,
  T_JUMP_STMT,
  T_LABELED_STMT,
  T_LIST,
  T_LITERAL,
  T_PARAMETER_LIST,
  T_POINTER,
  T_POSITION_DECL,
  T_SELECT_STMT,
  T_SPECIFIER,
  T_STRUCT_DECL_LIST,
  T_STRUCT_DECLARATOR_LIST,
  T_TUPLE,
  T_UNIT,
} NodeType;

typedef enum LiteralKind
{
  INT = 0,
  STR,
} LiteralKind;

typedef enum ExpressionKind {
  PFX_EXPR_ARRAY = 0,
  PFX_EXPR_CALL,
  PFX_EXPR_FIELD_ACCESS,
  PFX_EXPR_POINTER_OP,
  PFX_EXPR_INCREMENT,
  PFX_EXPR_DECREMENT,
  PRE_EXPR_INCREMENT,
  PRE_EXPR_DECREMENT,
  BIN_EXPR_LOGICAL_OR,
  BIN_EXPR_LOGICAL_AND,
  BIN_EXPR_LOGICAL_XOR,
  BIN_EXPR_BINARY_OR,
  BIN_EXPR_BINARY_AND,
  BIN_EXPR_EQ,
  BIN_EXPR_NE,
  BIN_EXPR_LT,
  BIN_EXPR_GT,
  BIN_EXPR_LE,
  BIN_EXPR_GE,
  BIN_EXPR_SHL,
  BIN_EXPR_SHR,
  BIN_EXPR_ADD,
  BIN_EXPR_SUB,
  BIN_EXPR_MUL,
  BIN_EXPR_DIV,
  BIN_EXPR_MOD,
  UNARY_REF,     // &
  UNARY_DEREF,   // *
  UNARY_PLUS,    // +
  UNARY_MINUS,   // -
  UNARY_INV,     // ~
  UNARY_NOT,     // !
  EXPR_ASSIGN,      // =
  EXPR_MUL_ASSIGN,  // *=
  EXPR_DIV_ASSIGN,  // /=
  EXPR_MOD_ASSIGN,  // %=
  EXPR_ADD_ASSIGN,  // +=
  EXPR_SUB_ASSIGN,  // -=
  EXPR_LEFT_ASSIGN, // <<=
  EXPR_RIGHT_ASSIGN,// >>=
  EXPR_AND_ASSIGN,  // &=
  EXPR_XOR_ASSIGN,  // ^=
  EXPR_OR_ASSIGN,   // |=
  EXPR_SIZEOF,
} ExpressionKind;

typedef enum SpecifierKind {
  SPEC_STATIC = 0,
  SPEC_INLINE,
  SPEC_VOID,
  SPEC_INT8,
  SPEC_UINT8,
  SPEC_INT16,
  SPEC_UINT16,
  SPEC_STRUCT,
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

typedef struct ParseNode ParseNode;
typedef struct IDENTIFIER IDENTIFIER;
typedef struct LIST LIST;
typedef struct LITERAL LITERAL;

struct ParseNode {
  NodeType type;
  LIST *childs;
  ParseNode *parent;
  int line;
  char *file;
};

typedef struct LIST {
  ParseNode hdr;
  dynarray *list;
} LIST;

typedef struct EXPRESSION {
  ParseNode hdr;
  ExpressionKind kind;
} EXPRESSION;

typedef struct ASSERT_DECL {
  ParseNode hdr;
  EXPRESSION *expr;
  LITERAL *message;
} ASSERT_DECL;

typedef struct LITERAL {
  ParseNode hdr;
  LiteralKind kind;
  int ival;
  char *strval;
} LITERAL;

struct IDENTIFIER {
  ParseNode hdr;
  char *name;
};

typedef struct POSITION_DECL {
  ParseNode hdr;
  char *file;
  int line;
} POSITION_DECL;

typedef struct LABELED_STMT {
  ParseNode hdr;
  LabeledStmtKind kind;
  ParseNode *name;
} LABELED_STMT;

typedef struct SPECIFIER {
  ParseNode hdr;
  SpecifierKind kind;
  IDENTIFIER *name;    // for structs
} SPECIFIER;

typedef struct DESIGNATOR {
  ParseNode hdr;
  DesignatorKind kind;
} DESIGNATOR;

typedef struct SELECT_STMT {
  ParseNode hdr;
  SelectStmtKind kind;
  ParseNode *expr;
} SELECT_STMT;

typedef struct ITERATION_STMT {
  ParseNode hdr;
  IterationStmtKind kind;
  ParseNode *init;
  ParseNode *cond;
  ParseNode *step;
} ITERATION_STMT;

typedef struct JUMP_STMT {
  ParseNode hdr;
  JumpStmtKind kind;
} JUMP_STMT;

typedef struct FUNCTION_DEF {
  ParseNode hdr;
  LIST *specifiers;
  ParseNode *declarator;
  LIST *statements;
} FUNCTION_DEF;

typedef struct CAST {
  ParseNode hdr;
  ParseNode *cast_to;
} CAST;

static inline const char *parse_node_type_str(NodeType type) {
  switch (type) {
    case T_Node:
      return "<>";
    case T_ASSERT_DECL:
      return "ASSERT_DECL";
    case T_BLOCK_LIST:
      return "BLOCK_LIST";
    case T_CAST:
      return "CAST";
    case T_DESIGNATOR:
      return "DESIGNATOR";
    case T_EXPRESSION:
      return "EXPRESSION";
    case T_FUNCTION_DEF:
      return "FUNCTION_DEF";
    case T_IDENTIFIER:
      return "IDENTIFIER";
    case T_ITERATION_STMT:
      return "ITERATION_STMT";
    case T_JUMP_STMT:
      return "JUMP_STMT";
    case T_LABELED_STMT:
      return "LABELED_STMT";
    case T_LIST:
      return "LIST";
    case T_LITERAL:
      return "LITERAL";
    case T_TUPLE:
      return "TUPLE";
    case T_POINTER:
      return "POINTER";
    case T_POSITION_DECL:
      return "POSITION_DECL";
    case T_SELECT_STMT:
      return "SELECT_STMT";
    case T_SPECIFIER:
      return "SPECIFIER";
    case T_DECL_LIST:
      return "DECL_LIST";
    case T_UNIT:
      return "UNIT";
    case T_DESIGNATOR_LIST:
      return "DESIGNATOR_LIST";
    case T_INITIALIZER_LIST:
      return "INITIALIZER_LIST";
    case T_IDENTIFIER_LIST:
      return "IDENTIFIER_LIST";
    case T_PARAMETER_LIST:
      return "PARAMETER_LIST";
    case T_STRUCT_DECL_LIST:
      return "STRUCT_DECL_LIST";
    case T_STRUCT_DECLARATOR_LIST:
      return "STRUCT_DECLARATOR_LIST";
    case T_INIT_DECLARATOR_LIST:
      return "INIT_DECLARATOR_LIST";
    case T_ARG_EXPR_LIST:
      return "ARG_EXPR_LIST";
    case T_EXPR_LIST:
      return "EXPR_LIST";
    case T_DECL_SPEC_LIST:
      return "DECL_SPEC_LIST";
  }
  return "???";
}

static inline const char *parse_node_expr_kind(ExpressionKind kind) {
  switch(kind) {
    case PFX_EXPR_ARRAY:
      return "ARRAY";
    case PFX_EXPR_CALL:
      return "CALL";
    case PFX_EXPR_FIELD_ACCESS:
      return "FIELD";
    case PFX_EXPR_POINTER_OP:
      return "PTR_OP";
    case PFX_EXPR_INCREMENT:
      return "POST++";
    case PFX_EXPR_DECREMENT:
      return "POST--";
    case PRE_EXPR_INCREMENT:
      return "++PRE";
    case PRE_EXPR_DECREMENT:
      return "--PRE";
    case BIN_EXPR_LOGICAL_OR:
      return "||";
    case BIN_EXPR_LOGICAL_AND:
      return "&&";
    case BIN_EXPR_LOGICAL_XOR:
      return "^";
    case BIN_EXPR_BINARY_OR:
      return "|";
    case BIN_EXPR_BINARY_AND:
      return "&";
    case BIN_EXPR_EQ:
      return "==";
    case BIN_EXPR_NE:
      return "!=";
    case BIN_EXPR_LT:
      return "<";
    case BIN_EXPR_GT:
      return ">";
    case BIN_EXPR_LE:
      return "<=";
    case BIN_EXPR_GE:
      return ">=";
    case BIN_EXPR_SHL:
      return "<<";
    case BIN_EXPR_SHR:
      return ">>";
    case BIN_EXPR_ADD:
      return "+";
    case BIN_EXPR_SUB:
      return "-";
    case BIN_EXPR_MUL:
      return "*";
    case BIN_EXPR_DIV:
      return "/";
    case BIN_EXPR_MOD:
      return "%";
    case UNARY_REF:
      return "& (ref)";
    case UNARY_DEREF:
      return "* (deref)";
    case UNARY_PLUS:
      return "+ (unary)";
    case UNARY_MINUS:
      return "- (unary)";
    case UNARY_INV:
      return "~";
    case UNARY_NOT:
      return "!";
    case EXPR_ASSIGN:
      return "=";
    case EXPR_MUL_ASSIGN:
      return "*=";
    case EXPR_DIV_ASSIGN:
      return "/=";
    case EXPR_MOD_ASSIGN:
      return "%=";
    case EXPR_ADD_ASSIGN:
      return "+=";
    case EXPR_SUB_ASSIGN:
      return "-=";
    case EXPR_LEFT_ASSIGN:
      return "<<=";
    case EXPR_RIGHT_ASSIGN:
      return ">>=";
    case EXPR_AND_ASSIGN:
      return "&=";
    case EXPR_XOR_ASSIGN:
      return "^=";
    case EXPR_OR_ASSIGN:
      return "|=";
    case EXPR_SIZEOF:
      return "sizeof";
  }
  return "???";
}

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
    case SPEC_STRUCT:
      return "struct";
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

extern ParseNode *rc_parse_node_macro_holder;

#define CreateParseNode(t, first_line)                                         \
(                                                                              \
  rc_parse_node_macro_holder = (ParseNode *)xmalloc2(sizeof(t), #t),           \
  memset(rc_parse_node_macro_holder, 0, sizeof(t)),                            \
  rc_parse_node_macro_holder->type = (T_##t),                                  \
  rc_parse_node_macro_holder->childs = (LIST *)xmalloc2(sizeof(LIST), "LIST"), \
  memset(rc_parse_node_macro_holder->childs, 0, sizeof(LIST)),                 \
  rc_parse_node_macro_holder->childs->hdr.type = T_LIST,                       \
  rc_parse_node_macro_holder->line = first_line,                               \
  (t *)rc_parse_node_macro_holder                                              \
)

#define CreatePrimitiveParseNode(t, first_line)                                \
(                                                                              \
  rc_parse_node_macro_holder = (ParseNode *)xmalloc(sizeof(ParseNode)),        \
  memset(rc_parse_node_macro_holder, 0, sizeof(ParseNode)),                    \
  rc_parse_node_macro_holder->type = (T_##t),                                  \
  rc_parse_node_macro_holder->childs = (LIST *)xmalloc2(sizeof(LIST), "LIST"), \
  memset(rc_parse_node_macro_holder->childs, 0, sizeof(LIST)),                 \
  rc_parse_node_macro_holder->childs->hdr.type = T_LIST,                       \
  rc_parse_node_macro_holder->line = first_line,                               \
  rc_parse_node_macro_holder                                                   \
)

#define AddChild(_parent, _child)                                              \
  if (_child) {                                                                \
    ((ParseNode *)_parent)->childs->list =                                     \
    dynarray_append_ptr(((ParseNode *)_parent)->childs->list, _child);         \
    ((ParseNode *)_child)->parent = (ParseNode *)_parent;                      \
  }

typedef bool(*parse_tree_walker_fn)(ParseNode *, void *context);
extern bool parse_tree_walker(ParseNode *root, parse_tree_walker_fn walker, void *context);

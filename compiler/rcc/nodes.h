#pragma once

#include "common/dynarray.h"

typedef enum NodeType
{
  T_Node,

  // Generic types
  T_LIST,
  T_EXPRESSION,

  // Parse tree types
  T_ARG_EXPR_LIST,
  T_STATIC_ASSERT_DECL,
  T_BLOCK_LIST,
  T_CAST,
  T_DECL_LIST,
  T_DECL_SPEC_LIST,
  T_DESIGNATOR,
  T_DESIGNATOR_LIST,
  T_EXPR_LIST,
  T_FUNCTION_DEF,
  T_IDENTIFIER,
  T_IDENTIFIER_LIST,
  T_INIT_DECLARATOR_LIST,
  T_INITIALIZER_LIST,
  T_ITERATION_STMT,
  T_JUMP_STMT,
  T_LABELED_STMT,
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

  // Syntax types
  // ...
} NodeType;

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

typedef struct LIST LIST;
typedef struct Node Node;

struct Node {
  NodeType type;
  LIST *childs;
  Node *parent;
  int line;
  int pos;
  char *file;
};

typedef struct LIST {
  Node hdr;
  dynarray *list;
} LIST;

typedef struct EXPRESSION {
  Node hdr;
  ExpressionKind kind;
} EXPRESSION;

static inline const char *node_type_str(NodeType type) {
  switch (type) {
    case T_Node:
      return "<>";
    case T_STATIC_ASSERT_DECL:
      return "STATIC_ASSERT_DECL";
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

static inline const char *node_expr_kind(ExpressionKind kind) {
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

#define EXPRESSION_KIND(node) \
  (((EXPRESSION *)(node))->kind)

static inline bool node_has_childs(Node *node) {
  return node && (dynarray_length(node->childs->list) > 0);
}

static inline int node_num_childs(Node *node) {
  return node ? dynarray_length(node->childs->list) : 0;
}

static inline Node *node_get_child(Node *node, int idx) {
  return node ? (Node *)dfirst(dynarray_nth_cell(node->childs->list, idx)) : NULL;
}

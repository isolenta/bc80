#pragma once

#include "rcc/nodes.h"

typedef enum SymbolTypeKind {
  TYPE_UNASSIGNED = 0,
  TYPE_VOID,
  TYPE_INT8,
  TYPE_UINT8,
  TYPE_INT16,
  TYPE_UINT16,
  TYPE_BOOL,
  TYPE_STRUCT,
  TYPE_FUNCTION,
  TYPE_POINTER,
} SymbolTypeKind;

typedef enum SymbolScopeKind {
  SYM_SCOPE_GLOBAL = 0, // exported from compilation unit
  SYM_SCOPE_UNIT,       // current compilation unit (global static specifier)
} SymbolScopeKind;

static inline const char *symbol_type_kind(SymbolTypeKind kind) {
  switch(kind) {
    case TYPE_VOID:
      return "void";
    case TYPE_INT8:
      return "i8";
    case TYPE_UINT8:
      return "u8";
    case TYPE_INT16:
      return "i16";
    case TYPE_UINT16:
      return "u16";
    case TYPE_BOOL:
      return "bool";
    case TYPE_STRUCT:
      return "struct";
    case TYPE_FUNCTION:
      return "function";
    case TYPE_POINTER:
      return "ptr";
    default:
      return "???";
  }
  return "???";
}

static inline const char *symbol_scope_kind(SymbolScopeKind kind) {
  switch(kind) {
    case SYM_SCOPE_GLOBAL:
      return "global";
    case SYM_SCOPE_UNIT:
      return "unit";
  }
  return "???";
}

static inline SymbolTypeKind SpecifierTypeToSymbolType(SpecifierKind kind) {
  if (kind == SPEC_VOID)
    return TYPE_VOID;
  if (kind == SPEC_INT8)
    return TYPE_INT8;
  if (kind == SPEC_UINT8)
    return TYPE_UINT8;
  if (kind == SPEC_INT16)
    return TYPE_INT16;
  if (kind == SPEC_UINT16)
    return TYPE_UINT16;
  if (kind == SPEC_BOOL)
    return TYPE_BOOL;
  if (kind == SPEC_STRUCT)
    return TYPE_STRUCT;
  return TYPE_UNASSIGNED;
}

#define POINTER_SIZEOF 2

typedef struct type_decl_t type_decl_t;

struct type_decl_t {
  char *name;
  SymbolTypeKind kind;
  position_t declared_at;     // inherited from parse node
  int size;                   // sizeof() for this object
  bool size_is_ambiguous;     // true if one of struct field is ambiguous yet
  dynarray *entries;          // array of (symbol_t*) as struct entries
  type_decl_t *ptr_to;        // valid if kind is TYPE_POINTER
};

typedef struct func_decl_t {
  char *name;
  position_t declared_at;     // inherited from parse node
  type_decl_t *return_type;
  bool is_inline;
  dynarray *args;             // array of (type_decl_t*) as argument list types
  char *section;
} func_decl_t;

typedef struct symbol_t {
  char *name;
  position_t declared_at;     // inherited from parse node
  type_decl_t *type;
  int array_len;              // >0 if specified as array-of-type
  SymbolScopeKind scope;
  int align;
  char *section;
} symbol_t;

extern void declare_type(hashmap *decls_hm, char *name, int size, SymbolTypeKind kind);
extern type_decl_t *get_type_decl(rcc_ctx_t *ctx, SymbolTypeKind kind, char *name);
extern void capture_declarations(rcc_ctx_t *ctx, Node *node);
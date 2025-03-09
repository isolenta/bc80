#include "common/buffer.h"
#include "common/mmgr.h"
#include "common/hashmap.h"
#include "rccompiler/nodes.h"
#include "rccompiler/parse_nodes.h"
#include "rccompiler/rcc.h"
#include "rccompiler/semantics.h"

void declare_type(hashmap *decls_hm,
                    char *name,
                    int size,
                    SymbolTypeKind kind)
{
  type_decl_t *typ = (type_decl_t *)xmalloc(sizeof(type_decl_t));
  memset(typ, 0, sizeof(type_decl_t));
  typ->name = xstrdup(name);
  typ->size = size;
  typ->kind = kind;
  typ->size_is_ambiguous = false;
  hashmap_set(decls_hm, typ->name, typ);
}

// name is optional, only for structs
type_decl_t *get_type_decl(rcc_ctx_t *ctx, SymbolTypeKind kind, char *name)
{
  type_decl_t *type_decl = hashmap_get(ctx->type_decls, (kind == TYPE_STRUCT) ? name : (char *)symbol_type_kind(kind));
  if (type_decl == NULL)
    report_error(ctx, "undeclared type '%s%s'",
      (kind == TYPE_STRUCT) ? "struct " : "",
      (kind == TYPE_STRUCT) ? name : (char *)symbol_type_kind(kind));

  return type_decl;
}

static bool semantics_parse_tree_walker(Node *node, rcc_ctx_t *ctx)
{
  symbol_t *sym;

  if (node == NULL)
    return false;

  // capture global declarations:
  //  - global objects placements
  //  - function prototype declarations
  //  - struct forward declarations
  //  - struct unambiguous declarations
  if (node->type == T_SPECIFIER && node->parent && node->parent->type == T_UNIT) {
    capture_declarations(ctx, node);
    return false;
  }

  return parse_tree_walker(node, (parse_tree_walker_fn) semantics_parse_tree_walker, ctx);;
}

// transform parse tree to semantics objects
// acceptable for codegen
void do_semantics(rcc_ctx_t *ctx, void *parse_tree)
{
  ctx->global_symtab = hashmap_create(1024, "symtab");
  ctx->func_decls = hashmap_create(1024, "functions");
  ctx->type_decls = hashmap_create(1024, "types");

  // add primitive type declaraton entries
  declare_type(ctx->type_decls, "i8", 1, TYPE_INT8);
  declare_type(ctx->type_decls, "i16", 2, TYPE_INT16);
  declare_type(ctx->type_decls, "u8", 1, TYPE_UINT8);
  declare_type(ctx->type_decls, "u16", 2, TYPE_UINT16);
  declare_type(ctx->type_decls, "bool", 1, TYPE_BOOL);
  declare_type(ctx->type_decls, "void", 0, TYPE_VOID);

  semantics_parse_tree_walker(parse_tree, ctx);
}

void semantics_free(rcc_ctx_t *ctx)
{
  // TODO
}

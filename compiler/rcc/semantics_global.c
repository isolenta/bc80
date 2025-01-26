#include "common/buffer.h"
#include "common/mmgr.h"
#include "common/hashmap.h"
#include "rcc/nodes.h"
#include "rcc/parse_nodes.h"
#include "rcc/rcc.h"
#include "rcc/semantics.h"

struct specifiers_decl {
  SymbolTypeKind kind;
  SymbolScopeKind scope;
  bool is_inline;
  char *struct_name;
  char *section_name;
  int align;
  Node *decl_list;

  bool static_captured;
  bool type_captured;
  bool inline_captured;
  bool section_captured;
  bool align_captured;
};

struct pointer_state {
  Node *declarator;
  type_decl_t *type_decl;
};

static struct specifiers_decl capture_specifiers(rcc_ctx_t *ctx, SPECIFIER *root_spec);

// returns true if changes applied
static bool struct_update_size(rcc_ctx_t *ctx, type_decl_t *type_decl)
{
  bool changes_applied = false;
  dynarray_cell *dc;

  if (!type_decl->size_is_ambiguous)
    return false;

  // don't touch forward declaration entries
  if (type_decl->size_is_ambiguous && dynarray_length(type_decl->entries) == 0)
    return false;

  type_decl->size = 0;
  bool prev_ambiguity = type_decl->size_is_ambiguous;
  type_decl->size_is_ambiguous = false;
  foreach(dc, type_decl->entries) {
    symbol_t *entry = (symbol_t *)dfirst(dc);

    if (!entry->type->size_is_ambiguous)
      type_decl->size += entry->type->size;
    else
      type_decl->size_is_ambiguous = true;
  }

  // return true is we're fixed ambiguity during update
  return (prev_ambiguity == true) && (type_decl->size_is_ambiguous == false);
}

static int capture_array_declaration(rcc_ctx_t *ctx, Node *declarator, Node **var_id)
{
  assert(node_has_childs(declarator));

  *var_id = (Node *)dinitial(declarator->childs->list);

  if ((*var_id)->type != T_IDENTIFIER)
    report_error(ctx, "multidimension arrays are not supported");

  if (node_num_childs(declarator) < 2)
    report_error(ctx, "array size must be specified");

  Node *dimension = (Node *)dsecond(declarator->childs->list);
  if (dimension->type != T_LITERAL ||
    (dimension->type == T_LITERAL && ((LITERAL *)dimension)->kind != LITINT))
  {
    report_error(ctx, "array size must be integer");
  }

  int dimval = ((LITERAL *)dimension)->ival;
  if (dimval < 1)
    report_error(ctx, "array size can't be lesser then 1");

  return dimval;
}

static void capture_pointer(rcc_ctx_t *ctx, struct pointer_state *state)
{
  Node *declarator = state->declarator;
  type_decl_t *type_decl = state->type_decl;

  if (declarator->type == T_POINTER) {
    assert(node_has_childs(declarator));

    state->type_decl = (type_decl_t *)xmalloc(sizeof(type_decl_t));
    memset(state->type_decl, 0, sizeof(type_decl_t));

    state->type_decl->kind = TYPE_POINTER;
    state->type_decl->size = POINTER_SIZEOF;
    state->type_decl->ptr_to = type_decl;

    state->declarator = dinitial(declarator->childs->list);

    capture_pointer(ctx, state);
  }
}

// inplace: true if function modifies existing entry in hashmap
static type_decl_t *capture_struct_declaration(rcc_ctx_t *ctx,
                                                struct specifiers_decl *spdecl,
                                                bool *inplace)
{
  type_decl_t *struct_decl = NULL;

  if (spdecl->static_captured)
    report_error(ctx, "can't specify static for struct declaration");

  if (spdecl->inline_captured)
    report_error(ctx, "can't specify inline for struct declaration");

  if (spdecl->align_captured)
    report_error(ctx, "can't specify align for struct declaration");

  if (spdecl->section_captured)
    report_error(ctx, "can't specify section for struct declaration");

  // check if this struct was declared before
  struct_decl = (type_decl_t *)hashmap_search(ctx->type_decls, spdecl->struct_name, HASHMAP_FIND, NULL);
  if (struct_decl && dynarray_length(struct_decl->entries) > 0)
    report_error(ctx, "struct %s is already defined", spdecl->struct_name);

  *inplace = true;

  if (!struct_decl) {
    struct_decl = (type_decl_t *)xmalloc(sizeof(type_decl_t));
    memset(struct_decl, 0, sizeof(type_decl_t));

    struct_decl->kind = TYPE_STRUCT;
    struct_decl->name = spdecl->struct_name;

    *inplace = false;
  }

  struct_decl->size_is_ambiguous = false;

  int sum_size = 0;
  dynarray_cell *dc;
  foreach (dc, spdecl->decl_list->childs->list) {
    struct specifiers_decl entry_spdecl = capture_specifiers(ctx, (SPECIFIER *)dfirst(dc));

    dynarray_cell *dc0;
    foreach (dc0, entry_spdecl.decl_list->childs->list) {
      Node *declarator = (Node *)dfirst(dc0);
      IDENTIFIER *name;
      int array_len = 1;
      struct pointer_state pstate;

      // this creates new instance of type_decl_t for each field in struct_declarator_list
      type_decl_t *entry_type_decl = get_type_decl(ctx, entry_spdecl.kind, entry_spdecl.struct_name);

      // recursively capture all possible pointers (if any)
      pstate.declarator = declarator;
      pstate.type_decl = entry_type_decl;
      capture_pointer(ctx, &pstate);
      declarator = pstate.declarator;
      entry_type_decl = pstate.type_decl;

      if (declarator->type == T_EXPRESSION && EXPRESSION_KIND(declarator) == PFX_EXPR_ARRAY) {
        array_len = capture_array_declaration(ctx, declarator, (Node **)&name);
      } else {
        name = (IDENTIFIER *)declarator;
      }

      assert(name->hdr.type == T_IDENTIFIER);

      if (entry_type_decl->kind == TYPE_VOID)
        report_error(ctx, "can't use void type for struct field");

      if (entry_spdecl.kind == TYPE_STRUCT && entry_type_decl->size_is_ambiguous)
        struct_decl->size_is_ambiguous = true;
      else
        sum_size += entry_type_decl->size * array_len;

      symbol_t *struct_entry = (symbol_t *)xmalloc(sizeof(symbol_t));
      struct_entry->name = xstrdup(name->name);
      struct_entry->type = entry_type_decl;
      struct_entry->array_len = array_len;

      struct_decl->entries = dynarray_append_ptr(struct_decl->entries, struct_entry);
    }
  }

  struct_decl->size = sum_size;

  return struct_decl;
}

static void capture_specifier(rcc_ctx_t *ctx, SPECIFIER *spec, struct specifiers_decl *state)
{
  if (spec->kind == SPEC_STATIC) {
    state->scope = SYM_SCOPE_UNIT;
    state->static_captured = true;
  } else if (spec->kind == SPEC_INLINE) {
    if (state->inline_captured)
      report_error(ctx, "multiple inline specifiers");

    state->is_inline = true;
    state->inline_captured = true;
  } else if (spec->kind == SPEC_ALIGN) {
    if (state->align_captured)
      report_error(ctx, "multiple align specifiers");

    assert(spec->arg);

    if (spec->arg->type != T_LITERAL || ((LITERAL *)(spec->arg))->kind != LITINT)
      report_error(ctx, "only constant integer allowed as align() argument");

    LITERAL *lit = (LITERAL *)(spec->arg);
    state->align = lit->ival;
    state->align_captured = true;
  } else if (spec->kind == SPEC_SECTION) {
    if (state->section_captured)
      report_error(ctx, "multiple section specifiers");

    assert(spec->arg);

    LITERAL *lit = (LITERAL *)(spec->arg);
    state->section_name = xstrdup(lit->strval);
    state->section_captured = true;
  } else if (spec->kind == SPEC_STRUCT) {
    if (state->type_captured)
      report_error(ctx, "multiple type specifiers");

    state->struct_name = xstrdup(((IDENTIFIER *)(spec->name))->name);
    state->kind = TYPE_STRUCT;
    state->type_captured = true;
  } else /* primitive types */ {
    if (state->type_captured)
      report_error(ctx, "multiple type specifiers");
    state->kind = SpecifierTypeToSymbolType(spec->kind);
    state->type_captured = true;
  }

  dynarray_cell *dc;
  foreach (dc, spec->hdr.childs->list) {
    Node *child = (Node *)dfirst(dc);
    if ((child->type == T_INIT_DECLARATOR_LIST) ||
          (child->type == T_STRUCT_DECL_LIST) ||
          (child->type == T_STRUCT_DECLARATOR_LIST))
    {
      state->decl_list = child;
    }

    if (child->type == T_SPECIFIER)
      capture_specifier(ctx, (SPECIFIER *)child, state);
  }
}

// parse type specifier (possible nested)
static struct specifiers_decl capture_specifiers(rcc_ctx_t *ctx, SPECIFIER *root_spec)
{
  struct specifiers_decl spdecl;
  memset(&spdecl, 0, sizeof(struct specifiers_decl));

  capture_specifier(ctx, root_spec, &spdecl);

  return spdecl;
}

static func_decl_t *capture_function_declaration(rcc_ctx_t *ctx,
                                                  Node *declarator,
                                                  bool is_inline,
                                                  char *section,
                                                  type_decl_t *ret_type_decl)
{
  func_decl_t *func_decl = NULL;

  EXPRESSION *call_expr = (EXPRESSION *)declarator;
  assert(node_num_childs((Node *)call_expr) == 2);

  IDENTIFIER *func_name = (IDENTIFIER *)node_get_child((Node *)call_expr, 0);
  assert(func_name->hdr.type == T_IDENTIFIER);

  func_decl = xmalloc(sizeof(func_decl_t));
  memset(func_decl, 0, sizeof(func_decl_t));

  func_decl->name = xstrdup(func_name->name);
  func_decl->is_inline = is_inline;
  func_decl->section = section;
  func_decl->return_type = ret_type_decl;

  Node *plist = node_get_child((Node *)call_expr, 1);

  dynarray_cell *dc_arg;
  foreach (dc_arg, plist->childs->list) {
    SPECIFIER *spec = (SPECIFIER *)dfirst(dc_arg);

    assert(spec->hdr.type == T_SPECIFIER);

    char *type_name = spec->name ? ((IDENTIFIER *)spec->name)->name : NULL;
    type_decl_t *type_decl = get_type_decl(ctx, SpecifierTypeToSymbolType(spec->kind), type_name);

    if (node_has_childs((Node *)spec)) {
      struct pointer_state pstate;
      Node *child = dinitial(spec->hdr.childs->list);

      if (child->type == T_EXPRESSION && EXPRESSION_KIND(child) == PFX_EXPR_ARRAY)
        report_error(ctx, "can't use array as function argument");

      // recursively capture all possible pointers (if any)
      pstate.declarator = child;
      pstate.type_decl = type_decl;
      capture_pointer(ctx, &pstate);
      type_decl = pstate.type_decl;
    }

    if (type_decl->kind == TYPE_VOID)
      report_error(ctx, "can't use void type for function argument");

    if (type_decl->size_is_ambiguous)
      report_error(ctx, "using ambigous type '%s%s' while function declaration",
        (type_decl->kind == TYPE_STRUCT ? "struct " : ""),
        type_decl->name);

    func_decl->args = dynarray_append_ptr(func_decl->args, type_decl);
  }

  return func_decl;
}

static void capture_var_decl(rcc_ctx_t *ctx,
                              Node *declarator,
                              type_decl_t *type_decl,
                              int dimension,
                              struct specifiers_decl *spdecl)
{
  if (spdecl->inline_captured)
    report_error(ctx, "can't specify inline for variable declaration");

  symbol_t *var_decl = (symbol_t *)xmalloc(sizeof(symbol_t));
  memset(var_decl, 0, sizeof(symbol_t));

  var_decl->name = xstrdup(((IDENTIFIER *)declarator)->name);
  var_decl->type = type_decl;
  var_decl->align = spdecl->align_captured ? spdecl->align : 1;
  var_decl->section = spdecl->section_name;

  if (var_decl->type->size_is_ambiguous)
    report_error(ctx, "using ambigous type '%s%s' while variable declaration",
      (var_decl->type->kind == TYPE_STRUCT ? "struct " : ""),
      var_decl->type->name);

  var_decl->scope = spdecl->scope;
  var_decl->array_len = dimension > 0 ? dimension : 1;

  hashmap_search(ctx->global_symtab, var_decl->name, HASHMAP_INSERT, var_decl);
}

// appends objects to ctx's global_symtab, func_decls, type_decls
void capture_declarations(rcc_ctx_t *ctx, Node *node)
{
  assert(node->type == T_SPECIFIER);

  // evaluate specifiers flags and link to declaration list
  struct specifiers_decl spdecl = capture_specifiers(ctx, (SPECIFIER *)node);

  if (spdecl.kind == TYPE_UNASSIGNED)
    report_error(ctx, "type is not specified for declaration");

  // ambiguous struct declaration
  if (!spdecl.decl_list) {
    if (spdecl.kind == TYPE_STRUCT) {
      // this is struct forward declaration
      if (spdecl.static_captured)
        report_error(ctx, "can't specify static for struct declaration");

      if (spdecl.inline_captured)
        report_error(ctx, "can't specify inline for struct declaration");

      if (spdecl.align_captured)
        report_error(ctx, "can't specify align for struct declaration");

      if (spdecl.section_captured)
        report_error(ctx, "can't specify section for struct declaration");

      type_decl_t *struct_fwd_decl = (type_decl_t *)xmalloc(sizeof(type_decl_t));
      memset(struct_fwd_decl, 0, sizeof(type_decl_t));

      struct_fwd_decl->kind = TYPE_STRUCT;
      struct_fwd_decl->name = spdecl.struct_name;
      struct_fwd_decl->size_is_ambiguous = true;

      hashmap_search(ctx->type_decls, struct_fwd_decl->name, HASHMAP_INSERT, struct_fwd_decl);
      return;
    } else {
      // doesn't make sense to declare primitive type
      report_error(ctx, "declaration list is empty");
    }
  }

  // unaambiguous struct declaration
  if (spdecl.decl_list->type == T_STRUCT_DECL_LIST) {
    bool inplace = false;
    type_decl_t *struct_decl = capture_struct_declaration(ctx, &spdecl, &inplace);
    if (!inplace)
      hashmap_search(ctx->type_decls, struct_decl->name, HASHMAP_INSERT, struct_decl);

    // update size and ambiguity flags in all structs (place to future optimizations)
    hashmap_scan *scan = NULL;
    hashmap_entry *entry = NULL;
    bool updates_applied = true;

    while (updates_applied) {
      updates_applied = false;
      scan = hashmap_scan_init(ctx->type_decls);
      while ((entry = hashmap_scan_next(scan)) != NULL) {
        type_decl_t *type_decl = (type_decl_t *)entry->value;

        if (type_decl->kind == TYPE_STRUCT) {
          if (struct_update_size(ctx, type_decl))
            updates_applied = true;
        }
      }
    }

    return;
  }

  assert(spdecl.decl_list->type == T_INIT_DECLARATOR_LIST);

  type_decl_t *type_decl = get_type_decl(ctx, spdecl.kind, spdecl.struct_name);

  // loop through declaration list item
  dynarray_cell *dc;
  foreach (dc, spdecl.decl_list->childs->list) {
    Node *declarator = (Node *)dfirst(dc);
    struct pointer_state pstate;

    // recursively capture all possible pointers (if any)
    pstate.declarator = declarator;
    pstate.type_decl = type_decl;
    capture_pointer(ctx, &pstate);
    declarator = pstate.declarator;
    type_decl = pstate.type_decl;

    if ((declarator->type == T_EXPRESSION) && (EXPRESSION_KIND(declarator) == PFX_EXPR_CALL)) {
      func_decl_t *func_decl = capture_function_declaration(ctx,
                                                              declarator,
                                                              spdecl.is_inline,
                                                              spdecl.section_name,
                                                              type_decl);
      hashmap_search(ctx->func_decls, func_decl->name, HASHMAP_INSERT, func_decl);
    } else if ((declarator->type == T_EXPRESSION) && (EXPRESSION_KIND(declarator) == PFX_EXPR_ARRAY)) {
      Node *var_id;
      int dimval = capture_array_declaration(ctx, declarator, &var_id);
      capture_var_decl(ctx, var_id, type_decl, dimval, &spdecl);
    } else if (declarator->type == T_IDENTIFIER) {
      capture_var_decl(ctx, declarator, type_decl, 1, &spdecl);
    }
  } // foreach (childs)
}

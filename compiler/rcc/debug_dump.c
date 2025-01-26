#include <ctype.h>

#include "common/buffer.h"
#include "common/mmgr.h"
#include "common/hashmap.h"
#include "rcc/nodes.h"
#include "rcc/parse_nodes.h"
#include "rcc/rcc.h"
#include "rcc/semantics.h"

static void dump_type_decl(buffer *dump_buf, type_decl_t *typ, int indent, bool head_only, bool dump_with_ptr)
{
  dynarray_cell *dc;

  if (!head_only) {
    for (int i = 0; i < indent; i++)
      buffer_append_char(dump_buf, ' ');
  }

  if (typ->kind == TYPE_POINTER) {
    assert(typ->ptr_to);
    buffer_append_char(dump_buf, 'p');
    dump_type_decl(dump_buf, typ->ptr_to, indent, head_only, true);
    return;
  }

  buffer_append(dump_buf, "%s%s", typ->kind == TYPE_STRUCT ? "struct " : "", typ->name);

  if (head_only) {
    return;
  }

  buffer_append(dump_buf, " sizeof=%d %s\n",
    dump_with_ptr ? POINTER_SIZEOF : typ->size,
    typ->size_is_ambiguous ? "[ambigous]" : "");

  foreach(dc, typ->entries) {
    symbol_t *field = (symbol_t *)dfirst(dc);

    for (int i = 0; i < indent + 2; i++)
      buffer_append_char(dump_buf, ' ');

    buffer_append(dump_buf, ".%s", field->name);
    if (field->array_len > 1)
      buffer_append(dump_buf, "[%d]", field->array_len);
    buffer_append_char(dump_buf, ' ');
    dump_type_decl(dump_buf, field->type, 0, false, false);
  }
}

static void dump_symbol(buffer *dump_buf, symbol_t *sym, int indent)
{
  for (int i = 0; i < indent; i++)
    buffer_append_char(dump_buf, ' ');

  buffer_append(dump_buf, "%s: ", sym->name);
  dump_type_decl(dump_buf, sym->type, 0, true, false);

  if (sym->array_len > 1)
    buffer_append(dump_buf, "[%d]", sym->array_len);

  buffer_append(dump_buf, " scope:%s align:%d section:%s\n",
    symbol_scope_kind(sym->scope),
    sym->align,
    (sym->section ? sym->section : "default"));
}

static void dump_func_decl(buffer *dump_buf, func_decl_t *func, int indent)
{
  dynarray_cell *dc;

  for (int i = 0; i < indent; i++)
    buffer_append_char(dump_buf, ' ');

  buffer_append(dump_buf, "%s%s", func->name, (func->is_inline ? " inline " : ""));

  buffer_append_string(dump_buf, " (");
  foreach(dc, func->args) {
    dump_type_decl(dump_buf, (type_decl_t *)dfirst(dc), indent + 4, true, false);
    if (dc__state.i != dc__state.d->length - 1)
      buffer_append_string(dump_buf, ", ");
  }
  buffer_append_string(dump_buf, ") -> ");
  dump_type_decl(dump_buf, func->return_type, 0, true, false);

  if (func->section)
    buffer_append(dump_buf, " [%s]", func->section);

  buffer_append_char(dump_buf, '\n');
}

static void parse_node_dump(buffer *dest, int indent, const char *prefix, Node *node) {
  if (indent > 0)
    buffer_append_char(dest, '\n');

  for (int i = 0; i < indent; i++)
    buffer_append_char(dest, ' ');

  int buf_pos = buffer_append(dest, "%s", prefix);
  indent += (dest->len - buf_pos);

  if (!node) {
    buffer_append_string(dest, "<none>");
    return;
  }

  buffer_append(dest, "[%d:%d] %s ", node->line, node->pos, node_type_str(node->type));

  switch (node->type) {
    case T_STATIC_ASSERT_DECL: {
      STATIC_ASSERT_DECL *static_assert_decl = (STATIC_ASSERT_DECL *)node;
      parse_node_dump(dest, indent + 2, "expr: ", (Node *)static_assert_decl->expr);
      parse_node_dump(dest, indent + 2, "message: ", (Node *)static_assert_decl->message);
      break;
    }
    case T_CAST: {
      CAST *cast = (CAST *)node;
      parse_node_dump(dest, indent + 2, "to: ", (Node *)cast->cast_to);
      break;
    }
    case T_EXPRESSION: {
      EXPRESSION *expr = (EXPRESSION *)node;
      buffer_append(dest, "kind: %s", node_expr_kind(expr->kind));
      break;
    }
    case T_FUNCTION_DEF: {
      FUNCTION_DEF *func = (FUNCTION_DEF *)node;
      parse_node_dump(dest, indent + 2, "specifiers: ", (Node *)func->specifiers);
      parse_node_dump(dest, indent + 2, "declarator: ", (Node *)func->declarator);
      parse_node_dump(dest, indent + 2, "statements: ", (Node *)func->statements);
      break;
    }
    case T_LITERAL: {
      LITERAL *l = (LITERAL *)node;
      if (l->kind == LITSTR) {
        char *p = l->strval;
        buffer_append_string(dest, "\"");
        while (*p) {
          char c = *p++;
          if (isprint(c))
            buffer_append_char(dest, c);
          else
            buffer_append(dest, "<%02d>", (int)c);
        }
        buffer_append_char(dest, '"');
      }
      else if (l->kind == LITINT)
        buffer_append(dest, "%d", l->ival);
      else if (l->kind == LITBOOL)
        buffer_append(dest, "%s", (l->bval ? "true" : "false"));
      break;
    }
    case T_IDENTIFIER: {
      IDENTIFIER *id = (IDENTIFIER *)node;
      buffer_append(dest, "name=\"%s\"", id->name);
      break;
    }
    case T_JUMP_STMT: {
      JUMP_STMT *jump = (JUMP_STMT *)node;
      buffer_append(dest, "kind: %s", parse_node_jump_kind(jump->kind));
      break;
    }
    case T_SPECIFIER: {
      SPECIFIER *spec = (SPECIFIER *)node;
      buffer_append(dest, "kind: %s", parse_node_specifier_kind(spec->kind));

      if (spec->kind == SPEC_STRUCT) {
        buffer_append(dest, "; name: %s",
          ((spec->name && (((Node *)(spec->name))->type == T_IDENTIFIER)) ?
              ((IDENTIFIER *)spec->name)->name :
              "<none>")
        );
      }

      if (spec->arg)
        parse_node_dump(dest, 0, "; arg: ", spec->arg);

      break;
    }
    case T_LIST: {
      LIST *list = (LIST *)node;
      if (dynarray_length(list->list) > 0) {
        dynarray_cell *dc;
        foreach(dc, list->list) {
          parse_node_dump(dest, indent + 2, "-", (Node *)dfirst(dc));
        }
      }
      break;
    }
    case T_SELECT_STMT: {
      SELECT_STMT *select = (SELECT_STMT *)node;
      buffer_append(dest, "kind: %s", parse_node_select_kind(select->kind));
      parse_node_dump(dest, indent + 2, "expr: ", (Node *)select->expr);
      break;
    }
    case T_LABELED_STMT: {
      LABELED_STMT *labeled = (LABELED_STMT *)node;
      buffer_append(dest, "kind: %s; ", parse_node_labeled_kind(labeled->kind));
      parse_node_dump(dest, 0, "name: ", (Node *)labeled->name);
      break;
    }
    case T_ITERATION_STMT: {
      ITERATION_STMT *iter = (ITERATION_STMT *)node;
      buffer_append(dest, "kind: %s", parse_node_iteration_kind(iter->kind));
      parse_node_dump(dest, indent + 2, "init: ", (Node *)iter->init);
      parse_node_dump(dest, indent + 2, "cond: ", (Node *)iter->cond);
      parse_node_dump(dest, indent + 2, "step: ", (Node *)iter->step);
      break;
    }
    case T_POSITION_DECL: {
      POSITION_DECL *pos_decl = (POSITION_DECL *)node;
      buffer_append(dest, "file: %s; line: %d", pos_decl->file, pos_decl->line);
      break;
    }
    case T_DESIGNATOR: {
      DESIGNATOR *designator = (DESIGNATOR *)node;
      buffer_append(dest, "kind: %s", parse_node_designator_kind(designator->kind));
      break;
    }

    /* primitive nodes follows, i.e. nodes without inner fields (only childs allowed) */
    case T_Node:
    case T_ARG_EXPR_LIST:
    case T_BLOCK_LIST:
    case T_DECL_LIST:
    case T_DECL_SPEC_LIST:
    case T_DESIGNATOR_LIST:
    case T_EXPR_LIST:
    case T_IDENTIFIER_LIST:
    case T_INIT_DECLARATOR_LIST:
    case T_INITIALIZER_LIST:
    case T_PARAMETER_LIST:
    case T_POINTER:
    case T_STRUCT_DECL_LIST:
    case T_STRUCT_DECLARATOR_LIST:
    case T_TUPLE:
    case T_UNIT:
    default:
      break;
  }

  // dump childs
  if (dynarray_length(node->childs->list) > 0) {
    dynarray_cell *dc;
    foreach(dc, node->childs->list) {
      parse_node_dump(dest, indent + 2, "", (Node *)dfirst(dc));
    }
  }
}

char *dump_semantics(rcc_ctx_t *ctx)
{
  hashmap_scan *scan = NULL;
  hashmap_entry *entry = NULL;
  buffer *dump_buf = buffer_init();
  char *result;

  buffer_append_string(dump_buf, "SYMTAB:\n");
  scan = hashmap_scan_init(ctx->global_symtab);
  while ((entry = hashmap_scan_next(scan)) != NULL) {
    dump_symbol(dump_buf, (symbol_t *)entry->value, 2);
  }

  buffer_append_string(dump_buf, "FUNCTIONS:\n");
  scan = hashmap_scan_init(ctx->func_decls);
  while ((entry = hashmap_scan_next(scan)) != NULL) {
    dump_func_decl(dump_buf, (func_decl_t *)entry->value, 2);
  }

  buffer_append_string(dump_buf, "TYPES:\n");
  scan = hashmap_scan_init(ctx->type_decls);
  while ((entry = hashmap_scan_next(scan)) != NULL) {
    dump_type_decl(dump_buf, (type_decl_t *)entry->value, 2, false, false);
  }

  result = buffer_dup(dump_buf);
  buffer_free(dump_buf);
  return result;
}

char *dump_parse_tree(void *parse_tree) {
  char *result;
  buffer *dump_buf = buffer_init();

  parse_node_dump(dump_buf, 0, "", (Node *)parse_tree);

  buffer_append_char(dump_buf, '\n');
  result = buffer_dup(dump_buf);
  buffer_free(dump_buf);

  return result;
}

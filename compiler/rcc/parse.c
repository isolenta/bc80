#include <ctype.h>
#include <libgen.h>

#include "common/buffer.h"
#include "common/mmgr.h"
#include "rcc/parse_nodes.h"
#include "rcc/parser.h"
#include "rcc/rcc.h"

#include "rc_parser.tab.h"
#include "rc_lexer.yy.h"

ParseNode *rc_parse_node_macro_holder;

struct adj_pos_state {
  bool upd_offset;
  int line_offset;
  int base_line;
  char *current_file;
};

static void parse_node_dump(buffer *dest, int indent, const char *prefix, ParseNode *node) {
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

  buffer_append(dest, "[%d] %s ", node->line, parse_node_type_str(node->type));

  switch (node->type) {
    case T_ASSERT_DECL: {
      ASSERT_DECL *assert_decl = (ASSERT_DECL *)node;
      parse_node_dump(dest, indent + 2, "expr: ", (ParseNode *)assert_decl->expr);
      parse_node_dump(dest, indent + 2, "message: ", (ParseNode *)assert_decl->message);
      break;
    }
    case T_CAST: {
      CAST *cast = (CAST *)node;
      parse_node_dump(dest, indent + 2, "to: ", (ParseNode *)cast->cast_to);
      break;
    }
    case T_EXPRESSION: {
      EXPRESSION *expr = (EXPRESSION *)node;
      buffer_append(dest, "kind: %s", parse_node_expr_kind(expr->kind));
      break;
    }
    case T_FUNCTION_DEF: {
      FUNCTION_DEF *func = (FUNCTION_DEF *)node;
      parse_node_dump(dest, indent + 2, "specifiers: ", (ParseNode *)func->specifiers);
      parse_node_dump(dest, indent + 2, "declarator: ", (ParseNode *)func->declarator);
      parse_node_dump(dest, indent + 2, "statements: ", (ParseNode *)func->statements);
      break;
    }
    case T_LITERAL: {
      LITERAL *l = (LITERAL *)node;
      if (l->kind == STR) {
        char *p = l->strval;
        buffer_append_string(dest, "strval=\"");
        while (*p) {
          char c = *p++;
          if (isprint(c))
            buffer_append_char(dest, c);
          else
            buffer_append(dest, "<%02d>", (int)c);
        }
        buffer_append_char(dest, '"');
      }
      else if (l->kind == INT)
        buffer_append(dest, "ival=\"%d\"", l->ival);
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
          ((spec->name && (((ParseNode *)(spec->name))->type == T_IDENTIFIER)) ?
              ((IDENTIFIER *)spec->name)->name :
              "<none>")
        );
      }

      break;
    }
    case T_LIST: {
      LIST *list = (LIST *)node;
      if (dynarray_length(list->list) > 0) {
        dynarray_cell *dc;
        foreach(dc, list->list) {
          parse_node_dump(dest, indent + 2, "-", (ParseNode *)dfirst(dc));
        }
      }
      break;
    }
    case T_SELECT_STMT: {
      SELECT_STMT *select = (SELECT_STMT *)node;
      buffer_append(dest, "kind: %s", parse_node_select_kind(select->kind));
      parse_node_dump(dest, indent + 2, "expr: ", (ParseNode *)select->expr);
      break;
    }
    case T_LABELED_STMT: {
      LABELED_STMT *labeled = (LABELED_STMT *)node;
      buffer_append(dest, "kind: %s; ", parse_node_labeled_kind(labeled->kind));
      parse_node_dump(dest, 0, "name: ", (ParseNode *)labeled->name);
      break;
    }
    case T_ITERATION_STMT: {
      ITERATION_STMT *iter = (ITERATION_STMT *)node;
      buffer_append(dest, "kind: %s", parse_node_iteration_kind(iter->kind));
      parse_node_dump(dest, indent + 2, "init: ", (ParseNode *)iter->init);
      parse_node_dump(dest, indent + 2, "cond: ", (ParseNode *)iter->cond);
      parse_node_dump(dest, indent + 2, "step: ", (ParseNode *)iter->step);
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
      parse_node_dump(dest, indent + 2, "", (ParseNode *)dfirst(dc));
    }
  }
}

static bool adjust_positions_walker(ParseNode *node, struct adj_pos_state *state) {
  if (node == NULL)
    return false;

  if (node->type == T_POSITION_DECL) {
    POSITION_DECL *pos_decl = (POSITION_DECL *)node;

    state->current_file = pos_decl->file;
    state->base_line = pos_decl->line;
    state->upd_offset = true;
  } else {
    if (state->upd_offset) {
      state->line_offset = node->line - state->base_line;
      state->upd_offset = false;
    }
    node->file = xstrdup(state->current_file);
    node->line -= state->line_offset;
  }

  return parse_tree_walker(node, (parse_tree_walker_fn) adjust_positions_walker, state);
}

static bool parse_node_free_walker(ParseNode *node, void *dummy) {
  if (node == NULL)
    return false;

  bool res = parse_tree_walker(node, (parse_tree_walker_fn) parse_node_free_walker, dummy);

  if (node->type == T_LITERAL) {
    LITERAL *l = (LITERAL *)node;
    if (l->kind == STR)
      xfree(l->strval);
  } else if (node->type == T_IDENTIFIER) {
    IDENTIFIER *id = (IDENTIFIER *)node;
    xfree(id->name);
  } else if (node->type == T_POSITION_DECL) {
    POSITION_DECL *pos_decl = (POSITION_DECL *)node;
    xfree(pos_decl->file);
  }

  xfree(node->file);
  xfree(node);

  return res;
}

/*
 * A walker routine should look like this:
 *
 * bool my_walker (ParseNode *node, my_struct *context)
 * {
 *  if (node == NULL)
 *    return false;
 *
 *  if (node->type == T_LITERAL)
 *  {
 *    ...
 *  }
 *  else if (node->type == T_EXPRESSION)
 *  {
 *    ...
 *  }
 *
 *  // for any node type not specially processed, do:
 *  return parse_tree_walker(node, my_walker, (void *) context);
 * }
 *
 * The "context" argument points to a struct that holds whatever context
 * information the walker routine needs --- it can be used to return data
 * gathered by the walker, too.  This argument is not touched by
 * parse_tree_walker, but it is passed down to recursive sub-invocations
 * of my_walker.  The tree walk is started from a setup routine that
 * fills in the appropriate context struct, calls my_walker with the top-level
 * node of the tree, and then examines the results.
 *
 * The walker routine should return "false" to continue the tree walk, or
 * "true" to abort the walk and immediately return "true" to the top-level
 * caller.
 */
bool
parse_tree_walker(ParseNode *node,
             parse_tree_walker_fn walker,
             void *context)
{
  if (node == NULL)
    return false;

  switch (node->type) {
    case T_ASSERT_DECL: {
      ASSERT_DECL *assert_decl = (ASSERT_DECL *)node;
      if (walker((ParseNode *)assert_decl->expr, context))
        return true;
      if (walker((ParseNode *)assert_decl->message, context))
        return true;
      break;
    }
    case T_CAST: {
      CAST *cast = (CAST *)node;
      if (walker((ParseNode *)cast->cast_to, context))
        return true;
      break;
    }
    case T_FUNCTION_DEF: {
      FUNCTION_DEF *func = (FUNCTION_DEF *)node;
      if (walker((ParseNode *)func->specifiers, context))
        return true;
      if (walker((ParseNode *)func->declarator, context))
        return true;
      if (walker((ParseNode *)func->statements, context))
        return true;
      break;
    }
    case T_LIST: {
      LIST *list = (LIST *)node;
      if (dynarray_length(list->list) > 0) {
        dynarray_cell *dc;
        foreach(dc, list->list) {
          if (walker((ParseNode *)dfirst(dc), context))
            return true;
        }
      }
      break;
    }
    case T_SELECT_STMT: {
      SELECT_STMT *select = (SELECT_STMT *)node;
      if (walker((ParseNode *)select->expr, context))
        return true;
      break;
    }
    case T_LABELED_STMT: {
      LABELED_STMT *labeled = (LABELED_STMT *)node;
      if (walker((ParseNode *)labeled->name, context))
        return true;
      break;
    }
    case T_ITERATION_STMT: {
      ITERATION_STMT *iter = (ITERATION_STMT *)node;
      if (walker((ParseNode *)iter->init, context))
        return true;
      if (walker((ParseNode *)iter->cond, context))
        return true;
      if (walker((ParseNode *)iter->step, context))
        return true;
      break;
    }

    /* primitive nodes follows, i.e. nodes without inner fields (only childs allowed) */
    case T_Node:
    case T_ARG_EXPR_LIST:
    case T_BLOCK_LIST:
    case T_DECL_LIST:
    case T_DECL_SPEC_LIST:
    case T_DESIGNATOR:
    case T_DESIGNATOR_LIST:
    case T_EXPR_LIST:
    case T_EXPRESSION:
    case T_IDENTIFIER:
    case T_IDENTIFIER_LIST:
    case T_INIT_DECLARATOR_LIST:
    case T_INITIALIZER_LIST:
    case T_JUMP_STMT:
    case T_LITERAL:
    case T_PARAMETER_LIST:
    case T_POINTER:
    case T_POSITION_DECL:
    case T_SPECIFIER:
    case T_STRUCT_DECL_LIST:
    case T_STRUCT_DECLARATOR_LIST:
    case T_TUPLE:
    case T_UNIT:
    default:
      break;
  }

  if (dynarray_length(node->childs->list) > 0) {
    dynarray_cell *dc;
    foreach(dc, node->childs->list) {
      if (walker((ParseNode *)dfirst(dc), context))
        return true;
    }
  }

  return false;
}

// stage 2: parse source text and produce parse tree
void *do_parse(rcc_ctx_t *ctx, char *source) {
  yyscan_t scanner;
  struct yy_buffer_state *bstate;
  struct adj_pos_state adj_pos_state;

  memset(&ctx->scanner_state, 0, sizeof(struct scanner_state));
  memset(&ctx->parser_state, 0, sizeof(struct parser_state));
  ctx->parse_tree_top = CreatePrimitiveParseNode(UNIT, 0);
  ctx->scanner_state.line_num = 1;
  ctx->scanner_state.nl_from_scanner = false;

  rc_lex_init(&scanner);
  rc_set_extra(&ctx->scanner_state, scanner);
  bstate = rc__scan_string(source, scanner);
  rc_parse(scanner, ctx);
  rc__delete_buffer(bstate, scanner);
  rc_lex_destroy(scanner);

  // adjust line and filename of nodes according #line nodes
  memset(&adj_pos_state, 0, sizeof(struct adj_pos_state));
  adjust_positions_walker(ctx->parse_tree_top, &adj_pos_state);

  return ctx->parse_tree_top;
}

char *dump_parse_tree(void *parse_tree) {
  char *result;
  buffer *dump_buf = buffer_init();

  parse_node_dump(dump_buf, 0, "", (ParseNode *)parse_tree);

  buffer_append_char(dump_buf, '\n');
  result = buffer_dup(dump_buf);
  buffer_free(dump_buf);

  return result;
}

void parse_tree_free(void *parse_tree) {
  parse_node_free_walker(parse_tree, NULL);
}

int parse_int(const char *str, int len, IntegerBase base)
{
  int result;
  char *tmp, *endptr;
  int basenum = 10;

  tmp = (char *)str;

  if (base == HEX) {
    tmp = (char*)str + 2;
    basenum = 16;
  }

  result = strtol(tmp, &endptr, basenum);
  if (endptr == tmp) {
    generic_report_error(NULL, 0, "error parse decimal integer: %s", str);
  }

  return result;
}

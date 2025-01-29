#include <ctype.h>
#include <libgen.h>

#include "common/buffer.h"
#include "common/mmgr.h"
#include "rccompiler/parse_nodes.h"
#include "rccompiler/parser.h"
#include "rccompiler/rcc.h"

#include "rc_parser.tab.h"
#include "rc_lexer.yy.h"

Node *rc_node_macro_holder;

struct adj_pos_state {
  bool upd_offset;
  int line_offset;
  int base_line;
  char *current_file;
};

static bool adjust_positions_walker(Node *node, struct adj_pos_state *state) {
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

static bool parse_node_free_walker(Node *node, void *dummy) {
  if (node == NULL)
    return false;

  bool res = parse_tree_walker(node, (parse_tree_walker_fn) parse_node_free_walker, dummy);

  if (node->type == T_LITERAL) {
    LITERAL *l = (LITERAL *)node;
    if (l->kind == LITSTR)
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
 * bool my_walker (Node *node, my_struct *context)
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
parse_tree_walker(Node *node,
             parse_tree_walker_fn walker,
             void *context)
{
  if (node == NULL)
    return false;

  switch (node->type) {
    case T_STATIC_ASSERT_DECL: {
      STATIC_ASSERT_DECL *static_assert_decl = (STATIC_ASSERT_DECL *)node;
      if (walker((Node *)static_assert_decl->expr, context))
        return true;
      if (walker((Node *)static_assert_decl->message, context))
        return true;
      break;
    }
    case T_CAST: {
      CAST *cast = (CAST *)node;
      if (walker((Node *)cast->cast_to, context))
        return true;
      break;
    }
    case T_FUNCTION_DEF: {
      FUNCTION_DEF *func = (FUNCTION_DEF *)node;
      if (walker((Node *)func->specifiers, context))
        return true;
      if (walker((Node *)func->declarator, context))
        return true;
      if (walker((Node *)func->statements, context))
        return true;
      break;
    }
    case T_LIST: {
      LIST *list = (LIST *)node;
      if (dynarray_length(list->list) > 0) {
        dynarray_cell *dc;
        foreach(dc, list->list) {
          if (walker((Node *)dfirst(dc), context))
            return true;
        }
      }
      break;
    }
    case T_SELECT_STMT: {
      SELECT_STMT *select = (SELECT_STMT *)node;
      if (walker((Node *)select->expr, context))
        return true;
      break;
    }
    case T_LABELED_STMT: {
      LABELED_STMT *labeled = (LABELED_STMT *)node;
      if (walker((Node *)labeled->name, context))
        return true;
      break;
    }
    case T_ITERATION_STMT: {
      ITERATION_STMT *iter = (ITERATION_STMT *)node;
      if (walker((Node *)iter->init, context))
        return true;
      if (walker((Node *)iter->cond, context))
        return true;
      if (walker((Node *)iter->step, context))
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
      if (walker((Node *)dfirst(dc), context))
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
  ctx->scanner_state.scan_standalone = false;
  ctx->scanner_state.rcc_ctx = ctx;
  ctx->scanner_state.source_ptr = source;

  ctx->current_position.filename = ctx->in_filename;

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
    report_error_noloc("error parse decimal integer: %s", str);
  }

  return result;
}

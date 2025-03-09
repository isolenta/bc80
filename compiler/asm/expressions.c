#include <assert.h>
#include <string.h>

#include "asm/compile.h"
#include "asm/parse.h"
#include "asm/render.h"
#include "common/buffer.h"
#include "common/hashmap.h"

static parse_node *eval_literal(compile_ctx_t *ctx, parse_node *node)
{
  assert(node->type == NODE_LITERAL);

  LITERAL *l = (LITERAL *)node;

  // replace DOLLAR-kind literal with current position value
  // everywhere but not in EQU statements
  if (ctx->node->type != NODE_EQU && l->kind == DOLLAR) {
    section_ctx_t *section = get_current_section(ctx);

    l->ival = section->curr_pc;
    l->kind = INT;
  }

  // replace single char string value with ASCII code (integer)
  if ((l->kind == STR) && strlen(l->strval) == 1) {
    l->ival = (int)l->strval[0];
    l->kind = INT;
  }

  // return any other literal as is
  return node;
}

static inline parse_node *
make_inherited_expr(EXPR *base, parse_node *newleft, parse_node *newright)
{
  EXPR *result = make_node_internal(EXPR);

  result->hdr.type = base->hdr.type;
  result->hdr.line = base->hdr.line;
  result->hdr.pos = base->hdr.pos;
  result->hdr.fn = base->hdr.fn;
  result->hdr.is_ref = base->hdr.is_ref;

  result->kind = base->kind;
  result->left = newleft;
  result->right = newright;

  return (parse_node *)result;
}

static inline parse_node *
make_int_literal(int value, bool is_ref)
{
  LITERAL *l = make_node_internal(LITERAL);

  l->kind = INT;
  l->hdr.is_ref = false;
  l->ival = value;

  return (parse_node *)l;
}

static inline int
get_int_literal_value(parse_node *l)
{
  assert(l->type == NODE_LITERAL);
  assert(((LITERAL *)l)->kind == INT);

  return ((LITERAL *)l)->ival;
}

// evaluate source expression to its simplest form
parse_node *expr_eval(compile_ctx_t *ctx, parse_node *node)
{
  if (node->type == NODE_LITERAL)
    return eval_literal(ctx, node);

  // try to resolve (replace to literal) identifier via symtab
  if (node->type == NODE_ID) {
    ID *id = (ID *)node;
    parse_node *nval = hashmap_get(ctx->symtab, id->name);
    if (nval) {
      if (nval->type == NODE_LITERAL)
        nval->is_ref = id->hdr.is_ref;
      return expr_eval(ctx, nval);
    } else {
      // There is a chance that it's label name inside REPT block.
      // Try to resolve it as suffixed label
      if (ctx->lookup_rept_suffix) {
        char *suffixed_name = bsprintf("%s#%s", id->name, ctx->lookup_rept_suffix);
        parse_node *nval2 = hashmap_get(ctx->symtab, suffixed_name);
        if (nval2) {
          if (nval2->type == NODE_LITERAL)
            nval2->is_ref = id->hdr.is_ref;
          return expr_eval(ctx, nval2);
        }
      }
    }
  }

  // can't to evaluate node: leave it as is
  if (node->type != NODE_EXPR)
    return node;

  EXPR *expr = (EXPR *)node;
  parse_node *result;

  if (expr->kind == SIMPLE) {
      // simple expression: just use inner node with preserving is_ref
      result = expr_eval(ctx, expr->left);
      result->is_ref = expr->hdr.is_ref;
      return result;
  }

  if (expr->kind == UNARY_PLUS) {
    // unary plus: just use inner node without preserving is_ref
    return expr_eval(ctx, expr->left);
  }

  // for the rest arithmetic expressions try to simplify them to literal form,
  // otherwise return expression copy with evaluated leafs

  parse_node *arg1 = expr_eval(ctx, expr->left);
  parse_node *arg2 = NULL;

  if (expr->kind != UNARY_MINUS && expr->kind != UNARY_INV &&  expr->kind != UNARY_NOT)
    arg2 = expr_eval(ctx, expr->right);

  if (expr->kind == BINARY_DIV && IS_INT_LITERAL(arg2) &&
    get_int_literal_value(arg2) == 0)
  {
    report_error(ctx, "division by zero");
  }

  bool can_eval_to_literal = false;
  if (expr->kind == UNARY_MINUS || expr->kind == UNARY_INV || expr->kind == UNARY_NOT)
    can_eval_to_literal = IS_INT_LITERAL(arg1);
  else
    can_eval_to_literal = IS_INT_LITERAL(arg1) && IS_INT_LITERAL(arg2);

  if (!can_eval_to_literal)
    return make_inherited_expr(expr, arg1, arg2);

  // now we sure that we can to evaluate expression to single literal
  ctx->was_literal_evals = true;

  switch (expr->kind) {
    case UNARY_MINUS:
      return make_int_literal(-1 * get_int_literal_value(arg1), expr->hdr.is_ref);

    case UNARY_INV:
      return make_int_literal(~get_int_literal_value(arg1), expr->hdr.is_ref);

    case UNARY_NOT:
      return make_int_literal(!get_int_literal_value(arg1), expr->hdr.is_ref);

    case BINARY_PLUS:
      return make_int_literal(
        get_int_literal_value(arg1) + get_int_literal_value(arg2),
        expr->hdr.is_ref);

    case BINARY_MINUS:
      return make_int_literal(
        get_int_literal_value(arg1) - get_int_literal_value(arg2),
        expr->hdr.is_ref);

    case BINARY_MUL:
      return make_int_literal(
        get_int_literal_value(arg1) * get_int_literal_value(arg2),
        expr->hdr.is_ref);

    case BINARY_DIV:
      return make_int_literal(
        get_int_literal_value(arg1) / get_int_literal_value(arg2),
        expr->hdr.is_ref);

    case BINARY_AND:
      return make_int_literal(
        get_int_literal_value(arg1) & get_int_literal_value(arg2),
        expr->hdr.is_ref);

    case BINARY_OR:
      return make_int_literal(
        get_int_literal_value(arg1) | get_int_literal_value(arg2),
        expr->hdr.is_ref);

    case BINARY_MOD:
      return make_int_literal(
        get_int_literal_value(arg1) % get_int_literal_value(arg2),
        expr->hdr.is_ref);

    case BINARY_SHL:
      return make_int_literal(
        get_int_literal_value(arg1) << get_int_literal_value(arg2),
        expr->hdr.is_ref);

    case BINARY_SHR:
      return make_int_literal(
        get_int_literal_value(arg1) >> get_int_literal_value(arg2),
        expr->hdr.is_ref);

    case COND_EQ:
      return make_int_literal(
        get_int_literal_value(arg1) == get_int_literal_value(arg2),
        expr->hdr.is_ref);

    case COND_NE:
      return make_int_literal(
        get_int_literal_value(arg1) != get_int_literal_value(arg2),
        expr->hdr.is_ref);

    case COND_LT:
      return make_int_literal(
        get_int_literal_value(arg1) < get_int_literal_value(arg2),
        expr->hdr.is_ref);

    case COND_LE:
      return make_int_literal(
        get_int_literal_value(arg1) <= get_int_literal_value(arg2),
        expr->hdr.is_ref);

    case COND_GT:
      return make_int_literal(
        get_int_literal_value(arg1) > get_int_literal_value(arg2),
        expr->hdr.is_ref);

    case COND_GE:
      return make_int_literal(
        get_int_literal_value(arg1) >= get_int_literal_value(arg2),
        expr->hdr.is_ref);

    default:
      break;
  }

  // unreachable
  abort();
  return NULL;
}

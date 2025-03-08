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

  result->type = base->type;
  result->line = base->line;
  result->pos = base->pos;
  result->fn = base->fn;
  result->kind = base->kind;
  result->is_ref = base->is_ref;

  result->left = newleft;
  result->right = newright;

  return (parse_node *)result;
}

static inline parse_node *
make_int_literal(int value, bool is_ref)
{
  LITERAL *l = make_node_internal(LITERAL);

  l->kind = INT;
  l->is_ref = false;
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
// literal_evals: set to true if any compile-time arithmetics were performed
parse_node *expr_eval(compile_ctx_t *ctx, parse_node *node, bool *literal_evals)
{
  EXPR *expr = (EXPR *)node;
  parse_node *result, *arg1, *arg2;

  if (node->type == NODE_LITERAL)
    return eval_literal(ctx, node);

  // try to resolve (replace to literal) identifier via symtab
  if (node->type == NODE_ID) {
    ID *id = (ID *)node;
    parse_node *nval = hashmap_search(ctx->symtab, id->name, HASHMAP_FIND, NULL);
    if (nval) {
      if (nval->type == NODE_LITERAL)
        ((LITERAL *)nval)->is_ref = id->is_ref;
      return expr_eval(ctx, nval, literal_evals);
    } else {
      // There is a chance that it's label name inside REPT block.
      // Try to resolve it as suffixed label
      if (ctx->lookup_rept_suffix) {
        char *suffixed_name = bsprintf("%s#%s", id->name, ctx->lookup_rept_suffix);
        parse_node *nval2 = hashmap_search(ctx->symtab, suffixed_name, HASHMAP_FIND, NULL);
        if (nval2) {
          if (nval2->type == NODE_LITERAL)
            ((LITERAL *)nval2)->is_ref = id->is_ref;
          return expr_eval(ctx, nval2, literal_evals);
        }
      }
    }
  }

  // can't to evaluate node: leave it as is
  if (node->type != NODE_EXPR)
    return node;

  // for expressions try to simplify them to literal form,
  // otherwise return expression copy with evaluated leafs
  switch (expr->kind) {
    case SIMPLE:
      // simple expression: kust copy inner node with preserving is_ref
      result = expr_eval(ctx, expr->left, literal_evals);

      if (result->type == NODE_LITERAL)
        ((LITERAL *)result)->is_ref = expr->is_ref;
      else if (result->type == NODE_ID)
        ((ID *)result)->is_ref = expr->is_ref;
      else if (result->type == NODE_EXPR)
        ((EXPR *)result)->is_ref = expr->is_ref;

      return result;

    case UNARY_PLUS:
      return expr_eval(ctx, expr->left, literal_evals);

    case UNARY_MINUS:
      arg1 = expr_eval(ctx, expr->left, literal_evals);

      if (IS_INT_LITERAL(arg1)) {
        result = make_int_literal(-1 * get_int_literal_value(arg1), expr->is_ref);
        if (literal_evals)
          *literal_evals = true;
        return result;
      }

      return make_inherited_expr(expr, arg1, NULL);

    case UNARY_NOT:
      arg1 = expr_eval(ctx, expr->left, literal_evals);

      if (IS_INT_LITERAL(arg1)) {
        result = make_int_literal(~get_int_literal_value(arg1), expr->is_ref);
        if (literal_evals)
          *literal_evals = true;
        return result;
      }

      return make_inherited_expr(expr, arg1, NULL);

    case BINARY_PLUS:
      arg1 = expr_eval(ctx, expr->left, literal_evals);
      arg2 = expr_eval(ctx, expr->right, literal_evals);

      if (IS_INT_LITERAL(arg1) && IS_INT_LITERAL(arg2)) {
        result = make_int_literal(
          get_int_literal_value(arg1) + get_int_literal_value(arg2),
          expr->is_ref);
        if (literal_evals)
          *literal_evals = true;
        return result;
      }

      return make_inherited_expr(expr, arg1, arg2);

    case BINARY_MINUS:
      arg1 = expr_eval(ctx, expr->left, literal_evals);
      arg2 = expr_eval(ctx, expr->right, literal_evals);

      if (IS_INT_LITERAL(arg1) && IS_INT_LITERAL(arg2)) {
        result = make_int_literal(
          get_int_literal_value(arg1) - get_int_literal_value(arg2),
          expr->is_ref);
        if (literal_evals)
          *literal_evals = true;
        return result;
      }

      return make_inherited_expr(expr, arg1, arg2);

    case BINARY_MUL:
      arg1 = expr_eval(ctx, expr->left, literal_evals);
      arg2 = expr_eval(ctx, expr->right, literal_evals);

      if (IS_INT_LITERAL(arg1) && IS_INT_LITERAL(arg2)) {
        result = make_int_literal(
          get_int_literal_value(arg1) * get_int_literal_value(arg2),
          expr->is_ref);
        if (literal_evals)
          *literal_evals = true;
        return result;
      }

      return make_inherited_expr(expr, arg1, arg2);

    case BINARY_DIV:
      arg1 = expr_eval(ctx, expr->left, literal_evals);
      arg2 = expr_eval(ctx, expr->right, literal_evals);

      if (get_int_literal_value(arg2) == 0)
        report_error(ctx, "division by zero");

      if (IS_INT_LITERAL(arg1) && IS_INT_LITERAL(arg2)) {
        result = make_int_literal(
          get_int_literal_value(arg1) / get_int_literal_value(arg2),
          expr->is_ref);
        if (literal_evals)
          *literal_evals = true;
        return result;
      }

      return make_inherited_expr(expr, arg1, arg2);

    case BINARY_AND:
      arg1 = expr_eval(ctx, expr->left, literal_evals);
      arg2 = expr_eval(ctx, expr->right, literal_evals);

      if (IS_INT_LITERAL(arg1) && IS_INT_LITERAL(arg2)) {
        result = make_int_literal(
          get_int_literal_value(arg1) & get_int_literal_value(arg2),
          expr->is_ref);
        if (literal_evals)
          *literal_evals = true;
        return result;
      }

      return make_inherited_expr(expr, arg1, arg2);

    case BINARY_OR:
      arg1 = expr_eval(ctx, expr->left, literal_evals);
      arg2 = expr_eval(ctx, expr->right, literal_evals);

      if (IS_INT_LITERAL(arg1) && IS_INT_LITERAL(arg2)) {
        result = make_int_literal(
          get_int_literal_value(arg1) | get_int_literal_value(arg2),
          expr->is_ref);
        if (literal_evals)
          *literal_evals = true;
        return result;
      }

      return make_inherited_expr(expr, arg1, arg2);

    case BINARY_MOD:
      arg1 = expr_eval(ctx, expr->left, literal_evals);
      arg2 = expr_eval(ctx, expr->right, literal_evals);

      if (IS_INT_LITERAL(arg1) && IS_INT_LITERAL(arg2)) {
        result = make_int_literal(
          get_int_literal_value(arg1) % get_int_literal_value(arg2),
          expr->is_ref);
        if (literal_evals)
          *literal_evals = true;
        return result;
      }

      return make_inherited_expr(expr, arg1, arg2);

    case BINARY_SHL:
      arg1 = expr_eval(ctx, expr->left, literal_evals);
      arg2 = expr_eval(ctx, expr->right, literal_evals);

      if (IS_INT_LITERAL(arg1) && IS_INT_LITERAL(arg2)) {
        result = make_int_literal(
          get_int_literal_value(arg1) << get_int_literal_value(arg2),
          expr->is_ref);
        if (literal_evals)
          *literal_evals = true;
        return result;
      }

      return make_inherited_expr(expr, arg1, arg2);

    case BINARY_SHR:
      arg1 = expr_eval(ctx, expr->left, literal_evals);
      arg2 = expr_eval(ctx, expr->right, literal_evals);

      if (IS_INT_LITERAL(arg1) && IS_INT_LITERAL(arg2)) {
        result = make_int_literal(
          get_int_literal_value(arg1) >> get_int_literal_value(arg2),
          expr->is_ref);
        if (literal_evals)
          *literal_evals = true;
        return result;
      }

      return make_inherited_expr(expr, arg1, arg2);

    case COND_EQ:
      arg1 = expr_eval(ctx, expr->left, literal_evals);
      arg2 = expr_eval(ctx, expr->right, literal_evals);

      if (IS_INT_LITERAL(arg1) && IS_INT_LITERAL(arg2)) {
        result = make_int_literal(
          get_int_literal_value(arg1) == get_int_literal_value(arg2),
          expr->is_ref);
        if (literal_evals)
          *literal_evals = true;
        return result;
      }

      return make_inherited_expr(expr, arg1, arg2);

    case COND_NE:
      arg1 = expr_eval(ctx, expr->left, literal_evals);
      arg2 = expr_eval(ctx, expr->right, literal_evals);

      if (IS_INT_LITERAL(arg1) && IS_INT_LITERAL(arg2)) {
        result = make_int_literal(
          get_int_literal_value(arg1) != get_int_literal_value(arg2),
          expr->is_ref);
        if (literal_evals)
          *literal_evals = true;
        return result;
      }

      return make_inherited_expr(expr, arg1, arg2);

    case COND_LT:
      arg1 = expr_eval(ctx, expr->left, literal_evals);
      arg2 = expr_eval(ctx, expr->right, literal_evals);

      if (IS_INT_LITERAL(arg1) && IS_INT_LITERAL(arg2)) {
        result = make_int_literal(
          get_int_literal_value(arg1) < get_int_literal_value(arg2),
          expr->is_ref);
        if (literal_evals)
          *literal_evals = true;
        return result;
      }

      return make_inherited_expr(expr, arg1, arg2);

    case COND_LE:
      arg1 = expr_eval(ctx, expr->left, literal_evals);
      arg2 = expr_eval(ctx, expr->right, literal_evals);

      if (IS_INT_LITERAL(arg1) && IS_INT_LITERAL(arg2)) {
        result = make_int_literal(
          get_int_literal_value(arg1) <= get_int_literal_value(arg2),
          expr->is_ref);
        if (literal_evals)
          *literal_evals = true;
        return result;
      }

      return make_inherited_expr(expr, arg1, arg2);

    case COND_GT:
      arg1 = expr_eval(ctx, expr->left, literal_evals);
      arg2 = expr_eval(ctx, expr->right, literal_evals);

      if (IS_INT_LITERAL(arg1) && IS_INT_LITERAL(arg2)) {
        result = make_int_literal(
          get_int_literal_value(arg1) > get_int_literal_value(arg2),
          expr->is_ref);
        if (literal_evals)
          *literal_evals = true;
        return result;
      }

      return make_inherited_expr(expr, arg1, arg2);

    case COND_GE:
      arg1 = expr_eval(ctx, expr->left, literal_evals);
      arg2 = expr_eval(ctx, expr->right, literal_evals);

      if (IS_INT_LITERAL(arg1) && IS_INT_LITERAL(arg2)) {
        result = make_int_literal(
          get_int_literal_value(arg1) >= get_int_literal_value(arg2),
          expr->is_ref);
        if (literal_evals)
          *literal_evals = true;
        return result;
      }

      return make_inherited_expr(expr, arg1, arg2);

    default:
      // forgot to add expr kind to switch?
      abort();
      break;
  }

  // unreachable
  abort();
  return NULL;
}

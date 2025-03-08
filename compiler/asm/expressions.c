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

// evaluate source expression to its simplest form
// literal_evals: set to true if any compile-time arithmetics were performed
parse_node *expr_eval(compile_ctx_t *ctx, parse_node *node, bool *literal_evals)
{
  if (node->type == NODE_LITERAL)
    return eval_literal(ctx, node);

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
      if (ctx->lookup_rept_iter_id != -1) {
        char *suffixed_name = bsprintf("%s#%d", id->name, ctx->lookup_rept_iter_id);
        parse_node *nval2 = hashmap_search(ctx->symtab, suffixed_name, HASHMAP_FIND, NULL);
        if (nval2) {
          if (nval2->type == NODE_LITERAL)
            ((LITERAL *)nval2)->is_ref = id->is_ref;
          return expr_eval(ctx, nval2, literal_evals);
        } else {
          return node;
        }
      }
      return node;
    }
  }

  assert (node->type == NODE_EXPR);

  EXPR *expr = (EXPR *)node;

  if (expr->kind == SIMPLE) {
    if (expr->left->type == NODE_LITERAL) {
      LITERAL *l = (LITERAL *)eval_literal(ctx, expr->left);
      l->is_ref = expr->is_ref;
      return (parse_node *)l;
    }
    else if (expr->left->type == NODE_ID) {
      ID *id = (ID *)expr->left;
      id->is_ref = expr->is_ref;
      parse_node *nval = hashmap_search(ctx->symtab, id->name, HASHMAP_FIND, NULL);
      if (nval) {
        if (nval->type == NODE_LITERAL)
          ((LITERAL *)nval)->is_ref = id->is_ref;
        return expr_eval(ctx, nval, literal_evals);
      }
      else
        return expr->left;
    }
  } else if (expr->kind == UNARY_PLUS) {
    return expr_eval(ctx, expr->left, literal_evals);
  } else if (expr->kind == UNARY_MINUS) {
    expr->left = expr_eval(ctx, expr->left, literal_evals);
    if (IS_INT_LITERAL(expr->left)) {
      LITERAL *result = make_node_internal(LITERAL);
      result->kind = INT;
      result->is_ref = expr->is_ref;
      result->ival = -((LITERAL *)expr->left)->ival;
      if (literal_evals)
        *literal_evals = true;
      return (parse_node *)result;
    } else {
      return node;
    }
  } else if (expr->kind == UNARY_NOT) {
    expr->left = expr_eval(ctx, expr->left, literal_evals);
    if (IS_INT_LITERAL(expr->left)) {
      LITERAL *result = make_node_internal(LITERAL);
      result->kind = INT;
      result->is_ref = expr->is_ref;
      result->ival = ~((LITERAL *)expr->left)->ival;
      if (literal_evals)
        *literal_evals = true;
      return (parse_node *)result;
    } else {
      return node;
    }
  } else if (expr->kind == BINARY_PLUS) {
    expr->left = expr_eval(ctx, expr->left, literal_evals);
    expr->right = expr_eval(ctx, expr->right, literal_evals);

    if (IS_INT_LITERAL(expr->left) && IS_INT_LITERAL(expr->right)) {
      LITERAL *result = make_node_internal(LITERAL);
      result->kind = INT;
      result->is_ref = expr->is_ref;
      result->ival = ((LITERAL *)expr->left)->ival + ((LITERAL *)expr->right)->ival;
      if (literal_evals)
        *literal_evals = true;
      return (parse_node *)result;
    } else {
      return node;
    }
  } else if (expr->kind == BINARY_MINUS) {
    expr->left = expr_eval(ctx, expr->left, literal_evals);
    expr->right = expr_eval(ctx, expr->right, literal_evals);

    if (IS_INT_LITERAL(expr->left) && IS_INT_LITERAL(expr->right)) {
      LITERAL *result = make_node_internal(LITERAL);
      result->kind = INT;
      result->is_ref = expr->is_ref;
      result->ival = ((LITERAL *)expr->left)->ival - ((LITERAL *)expr->right)->ival;
      if (literal_evals)
        *literal_evals = true;
      return (parse_node *)result;
    } else {
      return node;
    }
  } else if (expr->kind == BINARY_MUL) {
    expr->left = expr_eval(ctx, expr->left, literal_evals);
    expr->right = expr_eval(ctx, expr->right, literal_evals);

    if (IS_INT_LITERAL(expr->left) && IS_INT_LITERAL(expr->right)) {
      LITERAL *result = make_node_internal(LITERAL);
      result->kind = INT;
      result->is_ref = expr->is_ref;
      result->ival = ((LITERAL *)expr->left)->ival * ((LITERAL *)expr->right)->ival;
      if (literal_evals)
        *literal_evals = true;
      return (parse_node *)result;
    } else {
      return node;
    }
  } else if (expr->kind == BINARY_DIV) {
    expr->left = expr_eval(ctx, expr->left, literal_evals);
    expr->right = expr_eval(ctx, expr->right, literal_evals);

    if (IS_INT_LITERAL(expr->left) && IS_INT_LITERAL(expr->right)) {
      if (((LITERAL *)expr->right)->ival != 0) {
        LITERAL *result = make_node_internal(LITERAL);
        result->kind = INT;
        result->is_ref = expr->is_ref;
        result->ival = ((LITERAL *)expr->left)->ival / ((LITERAL *)expr->right)->ival;
        if (literal_evals)
          *literal_evals = true;
        return (parse_node *)result;
      } else
        report_error(ctx, "division by zero");
    } else {
      return node;
    }
  } else if (expr->kind == BINARY_AND) {
    expr->left = expr_eval(ctx, expr->left, literal_evals);
    expr->right = expr_eval(ctx, expr->right, literal_evals);

    if (IS_INT_LITERAL(expr->left) && IS_INT_LITERAL(expr->right)) {
      LITERAL *result = make_node_internal(LITERAL);
      result->kind = INT;
      result->is_ref = expr->is_ref;
      result->ival = ((LITERAL *)expr->left)->ival & ((LITERAL *)expr->right)->ival;
      if (literal_evals)
        *literal_evals = true;
      return (parse_node *)result;
    } else {
      return node;
    }
  } else if (expr->kind == BINARY_OR) {
    expr->left = expr_eval(ctx, expr->left, literal_evals);
    expr->right = expr_eval(ctx, expr->right, literal_evals);

    if (IS_INT_LITERAL(expr->left) && IS_INT_LITERAL(expr->right)) {
      LITERAL *result = make_node_internal(LITERAL);
      result->kind = INT;
      result->is_ref = expr->is_ref;
      result->ival = ((LITERAL *)expr->left)->ival | ((LITERAL *)expr->right)->ival;
      if (literal_evals)
        *literal_evals = true;
      return (parse_node *)result;
    } else {
      return node;
    }
  } else if (expr->kind == BINARY_MOD) {
    expr->left = expr_eval(ctx, expr->left, literal_evals);
    expr->right = expr_eval(ctx, expr->right, literal_evals);

    if (IS_INT_LITERAL(expr->left) && IS_INT_LITERAL(expr->right)) {
      LITERAL *result = make_node_internal(LITERAL);
      result->kind = INT;
      result->is_ref = expr->is_ref;
      result->ival = ((LITERAL *)expr->left)->ival % ((LITERAL *)expr->right)->ival;
      if (literal_evals)
        *literal_evals = true;
      return (parse_node *)result;
    } else {
      return node;
    }
  } else if (expr->kind == BINARY_SHL) {
    expr->left = expr_eval(ctx, expr->left, literal_evals);
    expr->right = expr_eval(ctx, expr->right, literal_evals);

    if (IS_INT_LITERAL(expr->left) && IS_INT_LITERAL(expr->right)) {
      LITERAL *result = make_node_internal(LITERAL);
      result->kind = INT;
      result->is_ref = expr->is_ref;
      result->ival = ((LITERAL *)expr->left)->ival << ((LITERAL *)expr->right)->ival;
      if (literal_evals)
        *literal_evals = true;
      return (parse_node *)result;
    } else {
      return node;
    }
  } else if (expr->kind == BINARY_SHR) {
    expr->left = expr_eval(ctx, expr->left, literal_evals);
    expr->right = expr_eval(ctx, expr->right, literal_evals);

    if (IS_INT_LITERAL(expr->left) && IS_INT_LITERAL(expr->right)) {
      LITERAL *result = make_node_internal(LITERAL);
      result->kind = INT;
      result->is_ref = expr->is_ref;
      result->ival = ((LITERAL *)expr->left)->ival >> ((LITERAL *)expr->right)->ival;
      if (literal_evals)
        *literal_evals = true;
      return (parse_node *)result;
    } else {
      return node;
    }
  }

  return node;
}

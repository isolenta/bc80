#include <stdio.h>
#include <stdarg.h>
#include <string.h>
#include <ctype.h>

#include "dynarray.h"
#include "hashmap.h"
#include "parse.h"
#include "common.h"
#include "render.h"
#include "compile.h"

static parse_node *eval_literal(compile_ctx_t *ctx, parse_node *node, bool do_eval_dollar) {
  assert(node->type == NODE_LITERAL);

  LITERAL *l = (LITERAL *)node;

  // replace DOLLAR-kind literal with current position value
  if (do_eval_dollar && l->kind == DOLLAR) {
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
parse_node *expr_eval(compile_ctx_t *ctx, parse_node *node, bool do_eval_dollar) {
  if (node->type == NODE_LITERAL)
    return eval_literal(ctx, node, do_eval_dollar);

  if (node->type == NODE_ID) {
    ID *id = (ID *)node;
    parse_node *nval = hashmap_search(ctx->symtab, id->name, HASHMAP_FIND, NULL);
    if (nval) {
      if (nval->type == NODE_LITERAL)
        ((LITERAL *)nval)->is_ref = id->is_ref;
      return expr_eval(ctx, nval, do_eval_dollar);
    } else {
      return node;
    }
  }

  assert (node->type == NODE_EXPR);

  EXPR *expr = (EXPR *)node;

  if (expr->kind == SIMPLE) {
    if (expr->left->type == NODE_LITERAL) {
      LITERAL *l = (LITERAL *)eval_literal(ctx, expr->left, do_eval_dollar);
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
        return expr_eval(ctx, nval, do_eval_dollar);
      }
      else
        return expr->left;
    }
  } else if (expr->kind == UNARY_PLUS) {
    return expr_eval(ctx, expr->left, do_eval_dollar);
  } else if (expr->kind == UNARY_MINUS) {
    expr->left = expr_eval(ctx, expr->left, do_eval_dollar);
    if ((expr->left->type == NODE_LITERAL) && (((LITERAL *)expr->left)->kind == INT)) {
      ((LITERAL *)expr->left)->ival = -((LITERAL *)expr->left)->ival;
      ((LITERAL *)expr->left)->is_ref = expr->is_ref;
      return (parse_node *)expr->left;
    } else {
      return node;
    }
  } else if (expr->kind == UNARY_NOT) {
    expr->left = expr_eval(ctx, expr->left, do_eval_dollar);
    if ((expr->left->type == NODE_LITERAL) && (((LITERAL *)expr->left)->kind == INT)) {
      ((LITERAL *)expr->left)->ival = ~((LITERAL *)expr->left)->ival;
      ((LITERAL *)expr->left)->is_ref = expr->is_ref;
      return (parse_node *)expr->left;
    } else {
      return node;
    }
  } else if (expr->kind == BINARY_PLUS) {
    expr->left = expr_eval(ctx, expr->left, do_eval_dollar);
    expr->right = expr_eval(ctx, expr->right, do_eval_dollar);

    if ((expr->left->type == NODE_LITERAL) && (((LITERAL *)expr->left)->kind == INT) &&
          (expr->right->type == NODE_LITERAL) && (((LITERAL *)expr->right)->kind == INT)) {
      ((LITERAL *)expr->left)->ival += ((LITERAL *)expr->right)->ival;
      ((LITERAL *)expr->left)->is_ref = expr->is_ref;
      return (parse_node *)expr->left;
    } else {
      return node;
    }
  } else if (expr->kind == BINARY_MINUS) {
    expr->left = expr_eval(ctx, expr->left, do_eval_dollar);
    expr->right = expr_eval(ctx, expr->right, do_eval_dollar);

    if ((expr->left->type == NODE_LITERAL) && (((LITERAL *)expr->left)->kind == INT) &&
          (expr->right->type == NODE_LITERAL) && (((LITERAL *)expr->right)->kind == INT)) {
      ((LITERAL *)expr->left)->ival -= ((LITERAL *)expr->right)->ival;
      ((LITERAL *)expr->left)->is_ref = expr->is_ref;
      return (parse_node *)expr->left;
    } else {
      return node;
    }
  } else if (expr->kind == BINARY_MUL) {
    expr->left = expr_eval(ctx, expr->left, do_eval_dollar);
    expr->right = expr_eval(ctx, expr->right, do_eval_dollar);

    if ((expr->left->type == NODE_LITERAL) && (((LITERAL *)expr->left)->kind == INT) &&
          (expr->right->type == NODE_LITERAL) && (((LITERAL *)expr->right)->kind == INT)) {
      ((LITERAL *)expr->left)->ival *= ((LITERAL *)expr->right)->ival;
      ((LITERAL *)expr->left)->is_ref = expr->is_ref;
      return (parse_node *)expr->left;
    } else {
      return node;
    }
  } else if (expr->kind == BINARY_DIV) {
    expr->left = expr_eval(ctx, expr->left, do_eval_dollar);
    expr->right = expr_eval(ctx, expr->right, do_eval_dollar);

    if ((expr->left->type == NODE_LITERAL) && (((LITERAL *)expr->left)->kind == INT) &&
          (expr->right->type == NODE_LITERAL) && (((LITERAL *)expr->right)->kind == INT)) {
      if (((LITERAL *)expr->right)->ival != 0)
        ((LITERAL *)expr->left)->ival /= ((LITERAL *)expr->right)->ival;
      else
        report_error(ctx, "division by zero\n");
      ((LITERAL *)expr->left)->is_ref = expr->is_ref;
      return (parse_node *)expr->left;
    } else {
      return node;
    }
  } else if (expr->kind == BINARY_AND) {
    expr->left = expr_eval(ctx, expr->left, do_eval_dollar);
    expr->right = expr_eval(ctx, expr->right, do_eval_dollar);

    if ((expr->left->type == NODE_LITERAL) && (((LITERAL *)expr->left)->kind == INT) &&
          (expr->right->type == NODE_LITERAL) && (((LITERAL *)expr->right)->kind == INT)) {
      ((LITERAL *)expr->left)->ival &= ((LITERAL *)expr->right)->ival;
      ((LITERAL *)expr->left)->is_ref = expr->is_ref;
      return (parse_node *)expr->left;
    } else {
      return node;
    }
  } else if (expr->kind == BINARY_OR) {
    expr->left = expr_eval(ctx, expr->left, do_eval_dollar);
    expr->right = expr_eval(ctx, expr->right, do_eval_dollar);

    if ((expr->left->type == NODE_LITERAL) && (((LITERAL *)expr->left)->kind == INT) &&
          (expr->right->type == NODE_LITERAL) && (((LITERAL *)expr->right)->kind == INT)) {
      ((LITERAL *)expr->left)->ival |= ((LITERAL *)expr->right)->ival;
      ((LITERAL *)expr->left)->is_ref = expr->is_ref;
      return (parse_node *)expr->left;
    } else {
      return node;
    }
  }

  return node;
}

static hashmap *make_symtab(hashmap *defineopts) {
  hashmap_scan *scan = NULL;
  hashmap_entry *entry = NULL;
  hashmap *symtab = hashmap_create(1024);

  scan = hashmap_scan_init(defineopts);
  while ((entry = hashmap_scan_next(scan)) != NULL) {
    LITERAL *l = (LITERAL *)make_node(LITERAL, 0);

    if (parse_any_integer(entry->value, &l->ival)) {
      l->kind = INT;
    } else {
      l->kind = STR;
      l->strval = strdup(entry->value);
    }

    hashmap_search(symtab, entry->key, HASHMAP_INSERT, l);
  }

  return symtab;
}

void compile(dynarray *parse, hashmap *defineopts, dynarray *includeopts, char *outfile, int target) {
  dynarray_cell *dc = NULL;

  compile_ctx_t compile_ctx;

  memset(&compile_ctx, 0, sizeof(compile_ctx));

  compile_ctx.symtab = make_symtab(defineopts);
  compile_ctx.verbose_error = true;
  compile_ctx.target = target;

  render_start(&compile_ctx);

  // ==================================================================================================
  // 1st pass: render code itself and collect patches to resolve forward declared constants at 2nd pass
  // ==================================================================================================

  foreach(dc, parse) {
    parse_node *node = (parse_node *)dfirst(dc);

    compile_ctx.node = node;

    if (node->type == NODE_EQU) {
      EQU *equ = (EQU *)node;
      hashmap_search(compile_ctx.symtab, equ->name->name, HASHMAP_INSERT, expr_eval(&compile_ctx, (parse_node *)equ->value, false));
    }

    if (node->type == NODE_END) {
      break;
    }

    if (node->type == NODE_ORG) {
      ORG *org = (ORG *)node;

      parse_node *org_val = expr_eval(&compile_ctx, org->value, true);

      if (org_val->type == NODE_LITERAL) {
        LITERAL *l = (LITERAL *)org_val;
        if (l->kind == INT) {
          section_ctx_t *section = get_current_section(&compile_ctx);

          section->curr_pc = l->ival;
          render_reorg(&compile_ctx);
        } else {
          report_error(&compile_ctx, "value must be an integer (got %s)\n", get_literal_kind(l));
        }
      } else {
        report_error(&compile_ctx, "value must be a literal (got %s: %s)\n", get_parse_node_name(org_val), node_to_string(org_val));
      }

    }

    if (node->type == NODE_DEF) {
      DEF *def = (DEF *)node;
      dynarray_cell *def_dc = NULL;

      if (def->kind == DEFKIND_DS) {
        parse_node *nrep, *filler;

        if (dynarray_length(def->values->list) == 1) {
          // if argument 2 is omitted let's fill block with zeros
          filler = (parse_node *)make_node(LITERAL, 0);
          ((LITERAL *)filler)->is_ref = false;
          ((LITERAL *)filler)->kind = INT;
          ((LITERAL *)filler)->ival = 0;
        } else if (dynarray_length(def->values->list) == 2) {
          // filler value can be evaluated here or later
          filler = expr_eval(&compile_ctx, dsecond(def->values->list), true);
        } else {
          report_error(&compile_ctx, "DEFS must contain 1 or 2 arguments\n");
        }

        // size of block must be explicit integer value
        nrep = expr_eval(&compile_ctx, dinitial(def->values->list), true);
        if ((nrep->type != NODE_LITERAL) || (((LITERAL *)nrep)->kind != INT))
          report_error(&compile_ctx, "argument 1 of DEFS must explicit integer literal value\n");

        int fill_ival = 0;
        if (filler->type == NODE_LITERAL) {
          if (((LITERAL *)filler)->kind != INT)
            report_error(&compile_ctx, "argument 2 of DEFS must have integer type\n");
          fill_ival = ((LITERAL *)filler)->ival;
        }

        render_block(&compile_ctx, fill_ival, ((LITERAL *)nrep)->ival);

        // remember position for further patching in case of unresolved filler identifier
        if (filler->type != NODE_LITERAL) {
          section_ctx_t *section = get_current_section(&compile_ctx);

          register_fwd_lookup(&compile_ctx,
                               filler,
                               section->curr_pc - ((LITERAL *)nrep)->ival,
                               ((LITERAL *)nrep)->ival,
                               false,
                               false,
                               0);
        }
      } else {
        // !DEFKIND_DS

        foreach(def_dc, def->values->list) {
          parse_node *def_elem = (parse_node *)dfirst(def_dc);
          def_elem = expr_eval(&compile_ctx, def_elem, true);

          LITERAL *l;

          if (def_elem->type == NODE_LITERAL)
            l = (LITERAL *)def_elem;
          else if (def_elem->type == NODE_ID) {
            // TODO: resolve from symtab here or on 2nd pass
            l = (LITERAL *)make_node(LITERAL, 0);
            l->kind = INT;
            l->ival = 0;
            l->is_ref = false;
          } else {
            report_error(&compile_ctx, "def list must contain only literal values\n");
          }

          if ((def->kind == DEFKIND_DB) || (def->kind == DEFKIND_DM)) {
            if (l->kind == INT)
              render_byte(&compile_ctx, l->ival);
            else if (l->kind == STR)
              render_bytes(&compile_ctx, l->strval, strlen(l->strval));
            else
              assert(0);
          } else if (def->kind == DEFKIND_DW) {
            if (l->kind != INT)
              report_error(&compile_ctx, "only integer literals allowed in DEFW statements\n");

            render_word(&compile_ctx, l->ival);
          }
        }
      } // !DEFKIND_DS
    }

    if (node->type == NODE_INCBIN) {
      INCBIN *incbin = (INCBIN *)node;
      if (incbin->filename->kind == STR)
        render_from_file(&compile_ctx, incbin->filename->strval, includeopts);
      else
        report_error(&compile_ctx, "incbin filename must be a string literal\n");
    }

    if (node->type == NODE_LABEL) {
      section_ctx_t *section = get_current_section(&compile_ctx);

      // actually label definition is just alias of current offset in the output stream
      LABEL *label = (LABEL *)node;

      LITERAL *cur_offset = make_node(LITERAL, 0);
      cur_offset->kind = INT;
      cur_offset->ival = section->curr_pc;

      hashmap_search(compile_ctx.symtab, label->name->name, HASHMAP_INSERT, cur_offset);
    }

    if (node->type == NODE_INSTR) {
      INSTR *instr = (INSTR *)node;
      dynarray_cell *dc = NULL;
      ID *name_id = (ID *)instr->name;
      char *name = name_id->name;

      // try to substitute instruction name from symtab
      LITERAL *l = hashmap_search(compile_ctx.symtab, name_id->name, HASHMAP_FIND, NULL);
      if (l != NULL) {
        if (l->kind == STR)
          name = l->strval;
        else
          report_error(&compile_ctx, "instruction name must be a string identifier\n");
      }

      // evaluate all instruction arguments
      if (instr->args && instr->args->list) {
        foreach(dc, instr->args->list) {
          dfirst(dc) = expr_eval(&compile_ctx, dfirst(dc), true);
        }
      }

      compile_instruction(&compile_ctx, name, instr->args);
    }
  }

  // ==================================================================================================
  // 2nd pass: patch code with unresolved constants values as soon as symtab is fully populated now
  // ==================================================================================================

  compile_ctx.verbose_error = false;

  foreach(dc, compile_ctx.patches) {
    patch_t *patch = (patch_t *)dfirst(dc);

    parse_node *resolved_node = expr_eval(&compile_ctx, patch->node, true);
    if (resolved_node->type != NODE_LITERAL)
      report_error(&compile_ctx, "unresolved symbol %s (%s)\n", node_to_string(patch->node), node_to_string(resolved_node));

    /// PATCH HERE
    LITERAL *l = (LITERAL *)resolved_node;
    if (l->kind != INT)
      report_error(&compile_ctx, "unexpected literal type at 2nd pass\n");

    render_patch(&compile_ctx, patch, l->ival);
  }

  render_save(&compile_ctx, outfile);
}

void report_error(compile_ctx_t *ctx, char *fmt, ...) {
  va_list args;

  fprintf(stderr, "\x1b[31m");

  if (ctx->verbose_error)
    fprintf(stderr, "%s error", get_parse_node_name(ctx->node));
  else
    fprintf(stderr, "Error");

  if (ctx->verbose_error)
    fprintf(stderr, " (at line %u)", ctx->node->line);

  fprintf(stderr, ": ");

  va_start(args, fmt);
  vfprintf(stderr, fmt, args);
  va_end(args);
  if (ctx->verbose_error)
    fprintf(stderr, "line was: \"%s\"\n", node_to_string(ctx->node));
  fprintf(stderr, "\x1b[0m");

  exit(1);
}

void report_warning(compile_ctx_t *ctx, char *fmt, ...) {
  va_list args;

  fprintf(stderr, "\x1b[33mwarning (at line %u): ", ctx->node->line);

  va_start(args, fmt);
  vfprintf(stderr, fmt, args);
  va_end(args);

  fprintf(stderr, "\x1b[0m");
}

void register_fwd_lookup(compile_ctx_t *ctx,
                          parse_node *unresolved_node,
                          uint32_t pos,
                          int nbytes,
                          bool negate,
                          bool was_reladdr,
                          uint32_t curr_pc_reladdr) {
  patch_t *patch = (patch_t *)malloc(sizeof(patch_t));

  patch->node = unresolved_node;
  patch->pos = pos;
  patch->nbytes = nbytes;
  patch->negate = negate;
  patch->was_reladdr = was_reladdr;
  patch->curr_pc_reladdr = curr_pc_reladdr;

  ctx->patches = dynarray_append_ptr(ctx->patches, patch);
}
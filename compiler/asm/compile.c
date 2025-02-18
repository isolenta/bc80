#include <stdint.h>
#include <stdio.h>
#include <string.h>

#include "asm/compile.h"
#include "asm/render.h"
#include "common/buffer.h"
#include "common/hashmap.h"

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
// literal_evals: set to true if any compile-time arithmetics were performed
parse_node *expr_eval(compile_ctx_t *ctx, parse_node *node, bool do_eval_dollar, bool *literal_evals) {
  if (node->type == NODE_LITERAL)
    return eval_literal(ctx, node, do_eval_dollar);

  if (node->type == NODE_ID) {
    ID *id = (ID *)node;
    parse_node *nval = hashmap_search(ctx->symtab, id->name, HASHMAP_FIND, NULL);
    if (nval) {
      if (nval->type == NODE_LITERAL)
        ((LITERAL *)nval)->is_ref = id->is_ref;
      return expr_eval(ctx, nval, do_eval_dollar, literal_evals);
    } else {
      // There is a chance that it's label name inside REPT block.
      // Try to resolve it as suffixed label
      if (ctx->lookup_rept_iter_id != -1) {
        char *suffixed_name = bsprintf("%s#%d", id->name, ctx->lookup_rept_iter_id);
        parse_node *nval2 = hashmap_search(ctx->symtab, suffixed_name, HASHMAP_FIND, NULL);
        if (nval2) {
          if (nval2->type == NODE_LITERAL)
            ((LITERAL *)nval2)->is_ref = id->is_ref;
          return expr_eval(ctx, nval2, do_eval_dollar, literal_evals);
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
        return expr_eval(ctx, nval, do_eval_dollar, literal_evals);
      }
      else
        return expr->left;
    }
  } else if (expr->kind == UNARY_PLUS) {
    return expr_eval(ctx, expr->left, do_eval_dollar, literal_evals);
  } else if (expr->kind == UNARY_MINUS) {
    expr->left = expr_eval(ctx, expr->left, do_eval_dollar, literal_evals);
    if ((expr->left->type == NODE_LITERAL) && (((LITERAL *)expr->left)->kind == INT)) {
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
    expr->left = expr_eval(ctx, expr->left, do_eval_dollar, literal_evals);
    if ((expr->left->type == NODE_LITERAL) && (((LITERAL *)expr->left)->kind == INT)) {
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
    expr->left = expr_eval(ctx, expr->left, do_eval_dollar, literal_evals);
    expr->right = expr_eval(ctx, expr->right, do_eval_dollar, literal_evals);

    if ((expr->left->type == NODE_LITERAL) && (((LITERAL *)expr->left)->kind == INT) &&
          (expr->right->type == NODE_LITERAL) && (((LITERAL *)expr->right)->kind == INT)) {
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
    expr->left = expr_eval(ctx, expr->left, do_eval_dollar, literal_evals);
    expr->right = expr_eval(ctx, expr->right, do_eval_dollar, literal_evals);

    if ((expr->left->type == NODE_LITERAL) && (((LITERAL *)expr->left)->kind == INT) &&
          (expr->right->type == NODE_LITERAL) && (((LITERAL *)expr->right)->kind == INT)) {
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
    expr->left = expr_eval(ctx, expr->left, do_eval_dollar, literal_evals);
    expr->right = expr_eval(ctx, expr->right, do_eval_dollar, literal_evals);

    if ((expr->left->type == NODE_LITERAL) && (((LITERAL *)expr->left)->kind == INT) &&
          (expr->right->type == NODE_LITERAL) && (((LITERAL *)expr->right)->kind == INT)) {
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
    expr->left = expr_eval(ctx, expr->left, do_eval_dollar, literal_evals);
    expr->right = expr_eval(ctx, expr->right, do_eval_dollar, literal_evals);

    if ((expr->left->type == NODE_LITERAL) && (((LITERAL *)expr->left)->kind == INT) &&
          (expr->right->type == NODE_LITERAL) && (((LITERAL *)expr->right)->kind == INT)) {
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
    expr->left = expr_eval(ctx, expr->left, do_eval_dollar, literal_evals);
    expr->right = expr_eval(ctx, expr->right, do_eval_dollar, literal_evals);

    if ((expr->left->type == NODE_LITERAL) && (((LITERAL *)expr->left)->kind == INT) &&
          (expr->right->type == NODE_LITERAL) && (((LITERAL *)expr->right)->kind == INT)) {
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
    expr->left = expr_eval(ctx, expr->left, do_eval_dollar, literal_evals);
    expr->right = expr_eval(ctx, expr->right, do_eval_dollar, literal_evals);

    if ((expr->left->type == NODE_LITERAL) && (((LITERAL *)expr->left)->kind == INT) &&
          (expr->right->type == NODE_LITERAL) && (((LITERAL *)expr->right)->kind == INT)) {
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
    expr->left = expr_eval(ctx, expr->left, do_eval_dollar, literal_evals);
    expr->right = expr_eval(ctx, expr->right, do_eval_dollar, literal_evals);

    if ((expr->left->type == NODE_LITERAL) && (((LITERAL *)expr->left)->kind == INT) &&
          (expr->right->type == NODE_LITERAL) && (((LITERAL *)expr->right)->kind == INT)) {
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
    expr->left = expr_eval(ctx, expr->left, do_eval_dollar, literal_evals);
    expr->right = expr_eval(ctx, expr->right, do_eval_dollar, literal_evals);

    if ((expr->left->type == NODE_LITERAL) && (((LITERAL *)expr->left)->kind == INT) &&
          (expr->right->type == NODE_LITERAL) && (((LITERAL *)expr->right)->kind == INT)) {
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
    expr->left = expr_eval(ctx, expr->left, do_eval_dollar, literal_evals);
    expr->right = expr_eval(ctx, expr->right, do_eval_dollar, literal_evals);

    if ((expr->left->type == NODE_LITERAL) && (((LITERAL *)expr->left)->kind == INT) &&
          (expr->right->type == NODE_LITERAL) && (((LITERAL *)expr->right)->kind == INT)) {
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

static hashmap *make_symtab(hashmap *defineopts) {
  hashmap_scan *scan = NULL;
  hashmap_entry *entry = NULL;
  hashmap *symtab = hashmap_create(1024, "symtab");

  if (!defineopts)
    return symtab;

  scan = hashmap_scan_init(defineopts);
  while ((entry = hashmap_scan_next(scan)) != NULL) {
    LITERAL *l = make_node_internal(LITERAL);

    if (parse_any_integer(entry->value, &l->ival)) {
      l->kind = INT;
    } else {
      l->kind = STR;
      l->strval = xstrdup(entry->value);
    }

    hashmap_search(symtab, entry->key, HASHMAP_INSERT, l);
  }

  return symtab;
}

int compile(struct libasm_as_desc_t *desc, dynarray *parse) {
  dynarray_cell *dc = NULL;

  compile_ctx_t compile_ctx;

  memset(&compile_ctx, 0, sizeof(compile_ctx));

  compile_ctx.symtab = make_symtab(desc->defineopts);
  compile_ctx.verbose_error = true;
  compile_ctx.target = desc->target;
  compile_ctx.sna_generic = desc->sna_generic;
  compile_ctx.sna_pc_addr = desc->sna_pc_addr;
  compile_ctx.sna_ramtop = desc->sna_ramtop;
  compile_ctx.lookup_rept_iter_id = -1;

  render_start(&compile_ctx);

  // ==================================================================================================
  // 1st pass: render code itself and collect patches to resolve forward declared constants at 2nd pass
  // ==================================================================================================

  foreach(dc, parse) {
    parse_node *node = (parse_node *)dfirst(dc);

    compile_ctx.node = node;

    if (node->type == NODE_EQU) {
      EQU *equ = (EQU *)node;
      hashmap_search(compile_ctx.symtab, equ->name->name, HASHMAP_INSERT, expr_eval(&compile_ctx, (parse_node *)equ->value, false, NULL));
    }

    if (node->type == NODE_END) {
      break;
    }

    if (node->type == NODE_SECTION) {
      // syntax: section name[,address[,filler]]
      SECTION *node_sect = (SECTION *)node;
      section_ctx_t *curr_section = get_current_section(&compile_ctx);
      dynarray_cell *dc;
      bool found = false;

      if (dynarray_length(node_sect->args->list) == 0)
        report_error(&compile_ctx, "'section' directive requires at least one argument");

      LITERAL *l = (LITERAL *)expr_eval(&compile_ctx, dinitial(node_sect->args->list), true, NULL);
      if (l->kind != STR)
        report_error(&compile_ctx, "section name must be a string");

      char *name = l->strval;

      // requesting current section is noop
      if (strcmp(name, curr_section->name) != 0) {
        // check section existance
        foreach (dc, compile_ctx.sections) {
          section_ctx_t *section = (section_ctx_t *)dfirst(dc);

          if (strcmp(name, section->name) == 0) {
            // requesting existing section is error
            bool old_v = compile_ctx.verbose_error;
            compile_ctx.verbose_error = false;
            report_error(&compile_ctx, "section '%s' already exists", name);
            compile_ctx.verbose_error = old_v;
          }
        }

        // by default new section seamless continues the current one
        int address = curr_section->curr_pc;

        if (dynarray_length(node_sect->args->list) > 1) {
          l = (LITERAL *)expr_eval(&compile_ctx, dfirst(dynarray_nth_cell(node_sect->args->list, 1)), true, NULL);
          if (l->kind != INT)
            report_error(&compile_ctx, "section address must be an integer");

          address = l->ival;
        }

        // by default new section's filler is zero
        uint8_t filler = 0;
        if (dynarray_length(node_sect->args->list) > 2) {
          l = (LITERAL *)expr_eval(&compile_ctx, dfirst(dynarray_nth_cell(node_sect->args->list, 2)), true, NULL);
          if (l->kind != INT)
            report_error(&compile_ctx, "section filler must be an integer");

          filler = (uint8_t)(l->ival & 0xff);
        }

        // create a new section
        section_ctx_t *new_sect = (section_ctx_t *)xmalloc(sizeof(section_ctx_t));

        new_sect->start = new_sect->curr_pc = address;
        new_sect->filler = filler;
        new_sect->name = xstrdup(name);
        new_sect->content = buffer_init();

        compile_ctx.sections = dynarray_append_ptr(compile_ctx.sections, new_sect);

        // make it current
        compile_ctx.curr_section_id = dynarray_length(compile_ctx.sections) - 1;
      }
    }

    if (node->type == NODE_ORG) {
      ORG *org = (ORG *)node;

      parse_node *org_val = expr_eval(&compile_ctx, org->value, true, NULL);

      if (org_val->type == NODE_LITERAL) {
        LITERAL *l = (LITERAL *)org_val;
        if (l->kind == INT) {
          section_ctx_t *section = get_current_section(&compile_ctx);

          if (l->ival < section->start) {
            bool old_v = compile_ctx.verbose_error;
            compile_ctx.verbose_error = false;
            report_error(&compile_ctx, "can't set ORG to %04xh because it's below section start address %04xh",
              l->ival, section->start);
            compile_ctx.verbose_error = old_v;
          }

          section->curr_pc = l->ival;
          render_reorg(&compile_ctx);
        } else {
          report_error(&compile_ctx, "value must be an integer (got %s)", get_literal_kind(l));
        }
      } else {
        report_error(&compile_ctx, "value must be a literal (got %s: %s)", get_parse_node_name(org_val), node_to_string(org_val));
      }

    }

    if (node->type == NODE_DEF) {
      DEF *def = (DEF *)node;
      dynarray_cell *def_dc = NULL;

      if (def->kind == DEFKIND_DS) {
        parse_node *nrep, *filler;

        if (dynarray_length(def->values->list) == 1) {
          // if argument 2 is omitted let's fill block with zeros
          filler = (parse_node *)make_node_internal(LITERAL);
          ((LITERAL *)filler)->is_ref = false;
          ((LITERAL *)filler)->kind = INT;
          ((LITERAL *)filler)->ival = 0;
        } else if (dynarray_length(def->values->list) == 2) {
          // filler value can be evaluated here or later
          filler = expr_eval(&compile_ctx, dsecond(def->values->list), true, NULL);
        } else {
          report_error(&compile_ctx, "DEFS must contain 1 or 2 arguments");
        }

        // size of block must be explicit integer value
        nrep = expr_eval(&compile_ctx, dinitial(def->values->list), true, NULL);
        if ((nrep->type != NODE_LITERAL) || (((LITERAL *)nrep)->kind != INT))
          report_error(&compile_ctx, "argument 1 of DEFS must explicit integer literal value");

        int fill_ival = 0;
        if (filler->type == NODE_LITERAL) {
          if (((LITERAL *)filler)->kind != INT)
            report_error(&compile_ctx, "argument 2 of DEFS must have integer type");
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
                               0);
        }
      } else {
        // !DEFKIND_DS

        foreach(def_dc, def->values->list) {
          parse_node *def_elem = (parse_node *)dfirst(def_dc);
          def_elem = expr_eval(&compile_ctx, def_elem, true, NULL);

          LITERAL *l;

          if (def_elem->type == NODE_LITERAL)
            l = (LITERAL *)def_elem;
          else if (def_elem->type == NODE_ID) {
            l = make_node_internal(LITERAL);
            l->kind = INT;
            l->ival = 0;
            l->is_ref = false;

            section_ctx_t *section = get_current_section(&compile_ctx);

            register_fwd_lookup(&compile_ctx,
                                 def_elem,
                                 section->curr_pc,
                                 (def->kind == DEFKIND_DW) ? 2 : 1,
                                 false,
                                 0);
          } else {
            report_error(&compile_ctx, "def list must contain only literal values");
          }

          if ((def->kind == DEFKIND_DB) || (def->kind == DEFKIND_DM)) {
            if (l->kind == INT)
              render_byte(&compile_ctx, l->ival, 0);
            else if (l->kind == STR)
              render_bytes(&compile_ctx, l->strval, strlen(l->strval));
            else
              assert(0);
          } else if (def->kind == DEFKIND_DW) {
            if (l->kind != INT)
              report_error(&compile_ctx, "only integer literals allowed in DEFW statements");

            render_word(&compile_ctx, l->ival);
          }
        }
      } // !DEFKIND_DS
    }

    if (node->type == NODE_INCBIN) {
      INCBIN *incbin = (INCBIN *)node;
      if (incbin->filename->kind == STR)
        render_from_file(&compile_ctx, incbin->filename->strval, desc->includeopts);
      else
        report_error(&compile_ctx, "incbin filename must be a string literal");
    }

    if (node->type == NODE_LABEL) {
      char *label_name;

      section_ctx_t *section = get_current_section(&compile_ctx);

      // actually label definition is just alias of current offset in the output stream
      LABEL *label = (LABEL *)node;

      label_name = label->name->name;

      // local label is just concatenation with global label of current scope
      if (label_name[0] == '.') {
        if (compile_ctx.curr_global_label == NULL)
          report_error(&compile_ctx, "can't assign local label '%s' without a global one", label_name);

        label_name = bsprintf("%s%s", compile_ctx.curr_global_label, label_name);
      } else {
        // label is not local => remember as scope label
        compile_ctx.curr_global_label = xstrdup(label_name);
      }

      // error out for label duplicates
      void *node = hashmap_search(compile_ctx.symtab, label_name, HASHMAP_FIND, NULL);
      if (node != NULL)
        report_error(&compile_ctx, "duplicate label '%s'", label_name);

      LITERAL *cur_offset = make_node_internal(LITERAL);
      cur_offset->kind = INT;
      cur_offset->ival = section->curr_pc;

      // if we're inside REPT block, localize label name by adding suffix
      // 'labelname' => 'labelname#n' where n is iteration index.
      // Note that symbol '#' is forbidden in identifiers so there is no way
      // to get a name collision with user-defined labels.
      char *localized_name = label_name;
      if (compile_ctx.curr_rept != NULL) {
        localized_name = bsprintf("%s#%d", label->name->name, compile_ctx.curr_rept->counter);
      }

      hashmap_search(compile_ctx.symtab, localized_name, HASHMAP_INSERT, cur_offset);
    }

    if (node->type == NODE_INSTR) {
      INSTR *instr = (INSTR *)node;
      dynarray_cell *dc = NULL;
      ID *name_id = (ID *)instr->name;
      char *name = name_id->name;

      // try to substitute instruction name from symtab
      LITERAL *l = hashmap_search(compile_ctx.symtab, name, HASHMAP_FIND, NULL);
      if (l != NULL) {
        if (l->kind == STR)
          name = l->strval;
        else
          report_error(&compile_ctx, "instruction name must be a string identifier");
      }

      // evaluate all instruction arguments
      if (instr->args && instr->args->list) {
        foreach(dc, instr->args->list) {
          bool literal_evals = false;
          parse_node *arg;
          bool is_ref = false;

          arg = expr_eval(&compile_ctx, dfirst(dc), true, &literal_evals);
          dfirst(dc) = arg;

          if (arg->type == NODE_LITERAL) {
            LITERAL *arg_lit = (LITERAL *)arg;
            is_ref = arg_lit->is_ref;
          }

          if (is_ref && literal_evals)
            report_warning(&compile_ctx, "argument with external parentheses is interpreted as an address");
        }

      }

      compile_instruction(&compile_ctx, name, instr->args);
    }

    if (node->type == NODE_REPT) {
      REPT *rept = (REPT *)node;
      LITERAL *count = rept->count;
      if (count->kind != INT)
        report_error(&compile_ctx, "REPT count must have integer type");

      if (compile_ctx.curr_rept != NULL)
        report_error(&compile_ctx, "nested REPT blocks are not allowed");

      compile_ctx.curr_rept = (rept_ctx_t *)xmalloc(sizeof(rept_ctx_t));
      compile_ctx.curr_rept->count = count->ival;
      compile_ctx.curr_rept->counter = 0;
      compile_ctx.curr_rept->start_iter = foreach_current_index(dc);
    }

    if (node->type == NODE_ENDR) {
      if (compile_ctx.curr_rept == NULL)
        report_error(&compile_ctx, "found ENDR directive outside of REPT block");

      if (compile_ctx.curr_rept->counter < compile_ctx.curr_rept->count - 1) {
        // rewind parse list foreach to first block statement
        compile_ctx.curr_rept->counter++;
        foreach_current_index(dc) = compile_ctx.curr_rept->start_iter;
        continue;
      } else {
        // repitition finished, stop looping and destroy context
        xfree(compile_ctx.curr_rept);
        compile_ctx.curr_rept = NULL;
      }
    }

    if (node->type == NODE_PROFILE) {
      PROFILE *pr = (PROFILE *)node;

      if (compile_ctx.in_profile)
        report_error(&compile_ctx, "nester PROFILE blocks are not allowed");

      LITERAL *name = pr->name;
      if (name->kind != STR)
        report_error(&compile_ctx, "PROFILE name must a string");

      memset(&compile_ctx.current_profile_data, 0, sizeof(compile_ctx.current_profile_data));
      compile_ctx.current_profile_name = xstrdup(name->strval);
      compile_ctx.in_profile = true;
    }

    if (node->type == NODE_ENDPROFILE) {
      if (!compile_ctx.in_profile)
        report_error(&compile_ctx, "found ENDPROFILE directive outside of PROFILE block");

      report_warning(&compile_ctx, "Profiling for '%s': %d bytes, %d cycles",
        compile_ctx.current_profile_name,
        compile_ctx.current_profile_data.bytes,
        compile_ctx.current_profile_data.cycles);

      xfree(compile_ctx.current_profile_name);
      compile_ctx.in_profile = false;
    }
  }

  // ==================================================================================================
  // 2nd pass: patch code with unresolved constants values as soon as symtab is fully populated now
  // ==================================================================================================

  foreach(dc, compile_ctx.patches) {
    patch_t *patch = (patch_t *)dfirst(dc);

    compile_ctx.lookup_rept_iter_id = patch->rept_iter_id;
    parse_node *resolved_node = expr_eval(&compile_ctx, patch->node, true, NULL);
    compile_ctx.lookup_rept_iter_id = -1;

    if (resolved_node->type != NODE_LITERAL) {
      parse_node *saved_node = compile_ctx.node;

      compile_ctx.node = resolved_node;
      report_error(&compile_ctx, "unresolved symbol %s", node_to_string(patch->node));
      compile_ctx.node = saved_node;
    }

    LITERAL *l = (LITERAL *)resolved_node;
    if (l->kind != INT)
      report_error(&compile_ctx, "unexpected literal type at 2nd pass");

    render_patch(&compile_ctx, patch, l->ival);
  }

  *desc->dest_size = render_finish(&compile_ctx, desc->dest);

  foreach (dc, compile_ctx.sections) {
    section_ctx_t *section = (section_ctx_t *)dfirst(dc);

    xfree(section->name);
    buffer_free(section->content);
    xfree(section);
  }

  // we are here, so there weren't errors during compilation
  return 0;
}

void register_fwd_lookup(compile_ctx_t *ctx,
                          parse_node *unresolved_node,
                          uint32_t pos,
                          int nbytes,
                          bool relative,
                          uint32_t instr_pc) {
  patch_t *patch = (patch_t *)xmalloc(sizeof(patch_t));

  if (unresolved_node->type == NODE_ID) {
    ID *id = (ID *)unresolved_node;

    if (id->name[0] == '.' && ctx->curr_global_label) {
      id->name = bsprintf("%s%s", ctx->curr_global_label, id->name);
    }
  }

  patch->node = unresolved_node;
  patch->pos = pos;
  patch->nbytes = nbytes;
  patch->relative = relative;
  patch->instr_pc = instr_pc;
  patch->section_id = ctx->curr_section_id;
  patch->rept_iter_id = ctx->curr_rept ? ctx->curr_rept->counter : -1;

  ctx->patches = dynarray_append_ptr(ctx->patches, patch);
}
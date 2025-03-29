#include <stdint.h>
#include <stdio.h>
#include <string.h>

#include "asm/bc80asm.h"
#include "asm/compile.h"
#include "asm/render.h"
#include "asm/symtab.h"
#include "bits/buffer.h"
#include "bits/hashmap.h"

static void profile_end(compile_ctx_t *ctx, bool fail_ok, char *next_label, bool profile_data)
{
  if (!ctx->in_profile) {
    if (fail_ok)
      return;

    report_error(ctx, "found ENDPROFILE directive outside of PROFILE block");
  }

  char *displayed_name;

  if (next_label) {
    displayed_name = bsprintf("%s -> %s", ctx->current_profile_name, next_label);
  } else {
    displayed_name = ctx->current_profile_name;
  }

  if (profile_data) {
    if (ctx->current_profile.cycles == 0 && ctx->current_profile.bytes > 0)
      report_info("\x1b[93mData block\x1b[97m '%s': %d bytes",
        ctx->current_profile_name,
        ctx->current_profile.bytes);
  }

  if (ctx->current_profile.cycles != 0)
    report_info("\x1b[96mCode block\x1b[97m '%s': %d bytes, %d cycles",
      displayed_name,
      ctx->current_profile.bytes,
      ctx->current_profile.cycles);

  xfree(ctx->current_profile_name);
  ctx->in_profile = false;
}

static void profile_start(compile_ctx_t *ctx, char *name)
{
  memset(&(ctx->current_profile), 0, sizeof(ctx->current_profile));
  ctx->current_profile_name = name;
  ctx->in_profile = true;
}

static void compile_equ(compile_ctx_t *ctx, EQU *equ)
{
  add_sym_variable_node(ctx, equ->name->name, expr_eval(ctx, (parse_node *)equ->value));
}

static void compile_section(compile_ctx_t *ctx, SECTION *section)
{
  // syntax: section name [base=addr, fill=val]

  dynarray_cell *dc = NULL;
  section_ctx_t *curr_section = get_current_section(ctx);

  assert(section->name->kind == STR);

  char *name = section->name->strval;

  // requesting current section is noop
  if (strcmp(name, curr_section->name) == 0)
    return;

  // check section existance
  foreach (dc, ctx->sections) {
    section_ctx_t *section = (section_ctx_t *)dfirst(dc);

    if (strcmp(name, section->name) == 0)
      report_error(ctx, "section '%s' already exists", name);
  }

  // section parameters defaults
  int address = curr_section->curr_pc;  // new section seamless continues the current one
  uint8_t fill = 0;                     // filler is zero byte

  if (section->params) {
    foreach(dc, section->params->list) {
      EQU *equ = (EQU *)dfirst(dc);
      assert(equ->hdr.type == NODE_EQU);

      if (strcasecmp(equ->name->name, "base") == 0) {
        LITERAL *base_value = (LITERAL *)expr_eval(ctx, (parse_node *)equ->value);
        if (!IS_INT_LITERAL(base_value))
          report_error(ctx, "can't evaluate 'base' parameter as integer value");

        address = base_value->ival;
        if (address < 0 || address > 0xffff)
          report_error(ctx, "'base' parameter value %d is out of bounds (0..65536)", address);
      } else if (strcasecmp(equ->name->name, "fill") == 0) {
        LITERAL *fill_value = (LITERAL *)expr_eval(ctx, (parse_node *)equ->value);
        if (!IS_INT_LITERAL(fill_value))
          report_error(ctx, "can't evaluate 'fill' parameter as integer value");

        fill = (uint8_t)(fill_value->ival & 0xff);

        if (fill_value->ival & (int)~0xff)
          report_warning(ctx, "cutting down 'fill' parameter value 0x%02x to 0x%02x",
            fill_value->ival, fill);
      } else {
        report_warning(ctx, "ignore unknown section parameter '%s'", equ->name->name);
      }
    }
  }

  // create a new section
  section_ctx_t *new_sect = (section_ctx_t *)xmalloc(sizeof(section_ctx_t));

  new_sect->start = new_sect->curr_pc = address;
  new_sect->filler = fill;
  new_sect->name = xstrdup(name);
  new_sect->content = buffer_init();

  ctx->sections = dynarray_append_ptr(ctx->sections, new_sect);

  // make it current
  ctx->curr_section_id = dynarray_length(ctx->sections) - 1;
}

static void compile_org(compile_ctx_t *ctx, ORG *org)
{
  parse_node *org_val = expr_eval(ctx, org->value);

  if (org_val->type == NODE_LITERAL) {
    LITERAL *l = (LITERAL *)org_val;
    if (l->kind == INT) {
      section_ctx_t *section = get_current_section(ctx);

      if (l->ival < 0 || l->ival > 0xffff) {
        report_error(ctx, "invalid value %d for ORG (must be in range 0..65535)",
          l->ival, l->ival);
      }

      if (l->ival < section->start) {
        report_error_noloc("can't set ORG to %04xh because it's below section start address %04xh",
          l->ival, section->start);
      }

      section->curr_pc = l->ival;
      render_reorg(ctx);
    } else {
      report_error(ctx, "value must be an integer (got %s)", get_literal_kind(l));
    }
  } else {
    report_error(ctx, "value must be a literal (got %s: %s)", get_parse_node_name(org_val), node_to_string(org_val));
  }
}

static void compile_def(compile_ctx_t *ctx, DEF *def)
{
  dynarray_cell *def_dc = NULL;

  if (def->kind == DEFKIND_DS) {
    parse_node *nrep, *filler;

    if (dynarray_length(def->values->list) == 1) {
      // if argument 2 is omitted let's fill block with zeros
      filler = (parse_node *)make_node_internal(LITERAL);
      filler->is_ref = false;
      ((LITERAL *)filler)->kind = INT;
      ((LITERAL *)filler)->ival = 0;
    } else if (dynarray_length(def->values->list) == 2) {
      // filler value can be evaluated here or later
      filler = expr_eval(ctx, dsecond(def->values->list));
    } else {
      report_error(ctx, "DEFS must contain 1 or 2 arguments");
    }

    // size of block must be explicit integer value
    nrep = expr_eval(ctx, dinitial(def->values->list));
    if ((nrep->type != NODE_LITERAL) || (((LITERAL *)nrep)->kind != INT))
      report_error(ctx, "argument 1 of DEFS must explicit integer literal value");

    int fill_ival = 0;
    if (filler->type == NODE_LITERAL) {
      if (((LITERAL *)filler)->kind != INT)
        report_error(ctx, "argument 2 of DEFS must have integer type");
      fill_ival = ((LITERAL *)filler)->ival;
    }

    render_block(ctx, fill_ival, ((LITERAL *)nrep)->ival);

    // remember position for further patching in case of unresolved filler identifier
    if (filler->type != NODE_LITERAL) {
      section_ctx_t *section = get_current_section(ctx);

      register_fwd_lookup(ctx,
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
      def_elem = expr_eval(ctx, def_elem);

      LITERAL *l;

      if (def_elem->type == NODE_LITERAL)
        l = (LITERAL *)def_elem;
      else if (def_elem->type == NODE_ID) {
        l = make_node_internal(LITERAL);
        l->kind = INT;
        l->ival = 0;
        l->hdr.is_ref = false;

        section_ctx_t *section = get_current_section(ctx);

        register_fwd_lookup(ctx,
                             def_elem,
                             section->curr_pc,
                             (def->kind == DEFKIND_DW) ? 2 : 1,
                             false,
                             0);
      } else {
        report_error(ctx, "def list must contain only literal values");
      }

      if ((def->kind == DEFKIND_DB) || (def->kind == DEFKIND_DM)) {
        if (l->kind == INT)
          render_byte(ctx, l->ival, 0);
        else if (l->kind == STR)
          render_bytes(ctx, l->strval, strlen(l->strval));
        else
          assert(0);
      } else if (def->kind == DEFKIND_DW) {
        if (l->kind != INT)
          report_error(ctx, "only integer literals allowed in DEFW statements");

        render_word(ctx, l->ival);
      }
    }
  } // !DEFKIND_DS
}

static void compile_incbin(compile_ctx_t *ctx, INCBIN *incbin)
{
  if (incbin->filename->kind == STR)
    render_from_file(ctx, incbin->filename->strval, g_includeopts);
  else
    report_error(ctx, "incbin filename must be a string literal");
}

static void compile_label(compile_ctx_t *ctx, int profile_mode, bool profile_data, LABEL *label)
{
  char *label_name;
  bool label_is_local = false;
  section_ctx_t *section = get_current_section(ctx);

  label_name = label->name->name;

  // local label is just concatenation with global label of current scope
  if (label_name[0] == '.') {
    if (ctx->curr_global_label == NULL)
      report_error(ctx, "can't assign local label '%s' without a global one", label_name);

    label_is_local = true;
    label_name = bsprintf("%s%s", ctx->curr_global_label, label_name);
  } else {
    // label is not local => remember as scope label
    ctx->curr_global_label = xstrdup(label_name);
  }

  // error out for label duplicates
  void *node = get_sym_variable(ctx, label_name, true);
  if (node != NULL)
    report_error(ctx, "duplicate label '%s'", label_name);

  // if we're inside REPT block, localize label name by adding suffix
  // 'labelname' => 'labelname#n1#n2#...' where nX is iteration index for
  // every nested REPT block starting from topmost one.
  // Note that symbol '#' is forbidden in identifiers so there is no way
  // to get a name collision with user-defined labels.
  char *localized_name = xstrdup(label_name);
  dynarray_cell *dc;
  foreach(dc, ctx->repts) {
    char *tmp = localized_name;
    rept_ctx_t *rept_ctx = (rept_ctx_t *)dfirst(dc);

    localized_name = bsprintf("%s#%d", localized_name, rept_ctx->counter);
    xfree(tmp);
  }

  add_sym_variable_integer(ctx, localized_name, section->curr_pc);

  xfree(localized_name);

  if (profile_mode != PROFILE_NONE) {
    if ((profile_mode == PROFILE_ALL) ||
      ((profile_mode == PROFILE_GLOBALS) && !label_is_local))
    {
      profile_end(ctx, true, label_name, profile_data);
      profile_start(ctx, xstrdup(label_name));
    }
  }
}

static void compile_instr(compile_ctx_t *ctx, INSTR *instr)
{
  dynarray_cell *dc = NULL;
  ID *name_id = (ID *)instr->name;
  char *name = name_id->name;
  dynarray *arglist = NULL;

  // evaluate all instruction arguments
  if (instr->args && instr->args->list) {
    foreach(dc, instr->args->list) {
      bool is_ref = false;

      ctx->was_literal_evals = false;

      parse_node *arg = expr_eval(ctx, dfirst(dc));
      arglist = dynarray_append_ptr(arglist, arg);

      if (arg->type == NODE_LITERAL) {
        LITERAL *arg_lit = (LITERAL *)arg;
        is_ref = arg_lit->hdr.is_ref;
      }

      if (is_ref && ctx->was_literal_evals)
        report_warning(ctx, "argument with external parentheses is interpreted as an address");
    }
  }

  compile_instruction_impl(ctx, name, arglist);
}

static void compile_rept(compile_ctx_t *ctx, REPT *rept, int loop_iter)
{
  LITERAL *count_value = (LITERAL *)expr_eval(ctx, (parse_node *)rept->count_expr);
  if (!IS_INT_LITERAL(count_value))
    report_error(ctx, "can't evaluate REPT argument as integer value");

  rept_ctx_t *rept_ctx = (rept_ctx_t *)xmalloc(sizeof(rept_ctx_t));
  rept_ctx->count = count_value->ival;
  rept_ctx->counter = 0;
  rept_ctx->start_iter = loop_iter;
  rept_ctx->varname = NULL;
  rept_ctx->rept_node_line = rept->hdr.line;

  if (rept->var) {
    void *existing_var = get_sym_variable(ctx, rept->var->name, true);
    if (existing_var != NULL)
      report_error(ctx, "can't redefine variable %s in REPT block", rept->var->name);

    add_sym_variable_integer(ctx, rept->var->name, rept_ctx->counter);
    rept_ctx->varname = xstrdup(rept->var->name);
  }

  // add new REPT block context
  ctx->repts = dynarray_append_ptr(ctx->repts, rept_ctx);
}

static bool compile_endr(compile_ctx_t *ctx, ENDR *endr, int *loop_iter)
{
  if (dynarray_length(ctx->repts) == 0)
    report_error(ctx, "found ENDR directive outside of REPT block");

  // use top REPT block context
  rept_ctx_t *rept_ctx = (rept_ctx_t *)dfirst(dynarray_last_cell(ctx->repts));

  if (rept_ctx->counter < rept_ctx->count - 1) {
    // rewind parse list foreach to first block statement
    rept_ctx->counter++;

    if (rept_ctx->varname) {
      LITERAL *counter_var = (LITERAL *)get_sym_variable(ctx, rept_ctx->varname, true);
      assert(counter_var);

      counter_var->ival = rept_ctx->counter;
    }

    *loop_iter = rept_ctx->start_iter;

    return true;
  }

  // repitition finished, stop looping and destroy context
  if (rept_ctx->varname) {
    LITERAL *l = (LITERAL *)remove_sym_variable(ctx, rept_ctx->varname);
    if (l)
      xfree(l);
  }

  dynarray_remove_last_cell(ctx->repts);
  xfree(rept_ctx);

  return false;
}

static void compile_profile(compile_ctx_t *ctx, int profile_mode, PROFILE *profile)
{
  if (profile_mode != PROFILE_NONE) {
    report_warning(ctx, "PROFILE directive ignored because '--profile' option was specified");
    return;
  }

  LITERAL *name = profile->name;
  if (name->kind != STR)
    report_error(ctx, "PROFILE name must a string");

  if (ctx->in_profile)
    report_error(ctx, "nested PROFILE blocks are not allowed");

  profile_start(ctx, xstrdup(name->strval));
}

static void compile_endprofile(compile_ctx_t *ctx, int profile_mode, ENDPROFILE *endprofile)
{
  if (profile_mode != PROFILE_NONE)
    profile_end(ctx, false, NULL, true);
}

static void compile_if(compile_ctx_t *ctx, IF *node_if)
{
  LITERAL *condition = (LITERAL *)expr_eval(ctx, (parse_node *)node_if->condition);
  if (!IS_INT_LITERAL(condition))
    report_error(ctx, "can't evaluate IF condition");

  condition_ctx_t *condition_ctx = (condition_ctx_t *)xmalloc(sizeof(condition_ctx_t));
  condition_ctx->expr = node_if->condition;
  condition_ctx->cond_value = (bool)(condition->ival);
  condition_ctx->inverse = false;
  condition_ctx->if_node_line = node_if->hdr.line;

  ctx->conditions = dynarray_append_ptr(ctx->conditions, condition_ctx);
}

static void compile_else(compile_ctx_t *ctx, ELSE *node_else)
{
  if (dynarray_length(ctx->conditions) == 0)
    report_error(ctx, "found ELSE without IF");

  condition_ctx_t *last_condition_ctx = (condition_ctx_t *)dfirst(dynarray_last_cell(ctx->conditions));
  if (last_condition_ctx->inverse == true)
    report_error(ctx, "multiple ELSE are not allowed");

  last_condition_ctx->inverse = true;
  last_condition_ctx->cond_value = !last_condition_ctx->cond_value;
}

static void compile_endif(compile_ctx_t *ctx, ENDIF *node_endif)
{
  if (dynarray_length(ctx->conditions) == 0)
    report_error(ctx, "found ENDIF without IF");

  condition_ctx_t *last_condition_ctx = (condition_ctx_t *)dfirst(dynarray_remove_last_cell(ctx->conditions));
  xfree(last_condition_ctx);
}

static bool condition_allow(compile_ctx_t *ctx)
{
  int num_conditions = dynarray_length(ctx->conditions);

  // no conditions exists: allow to compile all
  if (num_conditions == 0)
    return true;

  // we should check only last condition in the stack, because we couldn't
  // reach nested 'if' when outer condition block was disallowed
  condition_ctx_t *last_condition_ctx = (condition_ctx_t *)dfirst(dynarray_last_cell(ctx->conditions));
  return last_condition_ctx->cond_value;
}

uint32_t compile(compile_opts opts, hashmap *defineopts, dynarray *statements, char **dest_buf)
{
  dynarray_cell *dc = NULL;

  compile_ctx_t compile_ctx;

  memset(&compile_ctx, 0, sizeof(compile_ctx));

  compile_ctx.symtab = make_symtab(defineopts);
  compile_ctx.opts = opts;

  render_start(&compile_ctx);

  // ==================================================================================================
  // 1st pass: render code itself and collect patches to resolve forward declared constants at 2nd pass
  // ==================================================================================================

  foreach(dc, statements) {
    parse_node *node = (parse_node *)dfirst(dc);
    compile_ctx.node = node;

    // if current condition state is false, skip all statement except 'else' of 'endif'
    if (!condition_allow(&compile_ctx) &&
      !(node->type == NODE_ELSE || node->type == NODE_ENDIF))
    {
      continue;
    }

    switch (node->type) {
      case NODE_EQU:
        compile_equ(&compile_ctx, (EQU *)node);
        break;

      case NODE_SECTION:
        compile_section(&compile_ctx, (SECTION *)node);
        break;

      case NODE_ORG:
        compile_org(&compile_ctx, (ORG *)node);
        break;

      case NODE_DEF:
        compile_def(&compile_ctx, (DEF *)node);
        break;

      case NODE_INCBIN:
        compile_incbin(&compile_ctx, (INCBIN *)node);
        break;

      case NODE_LABEL:
        compile_label(&compile_ctx, opts.profile_mode, opts.profile_data, (LABEL *)node);
        break;

      case NODE_INSTR:
        compile_instr(&compile_ctx, (INSTR *)node);
        break;

      case NODE_REPT:
        compile_rept(&compile_ctx, (REPT *)node, foreach_current_index(dc));
        break;

      case NODE_ENDR:
        if (compile_endr(&compile_ctx, (ENDR *)node, &foreach_current_index(dc)))
          continue;
        break;

      case NODE_PROFILE:
        compile_profile(&compile_ctx, opts.profile_mode, (PROFILE *)node);
        break;

      case NODE_ENDPROFILE:
        compile_endprofile(&compile_ctx, opts.profile_mode, (ENDPROFILE *)node);
        break;

      case NODE_IF:
        compile_if(&compile_ctx, (IF *)node);
        break;

      case NODE_ELSE:
        compile_else(&compile_ctx, (ELSE *)node);
        break;

      case NODE_ENDIF:
        compile_endif(&compile_ctx, (ENDIF *)node);
        break;
      default:
        break;
    }

    if (node->type == NODE_END)
      break;
  }

  if (opts.profile_mode != PROFILE_NONE) {
    // flush the last profile block if any
    profile_end(&compile_ctx, true, "EOF", opts.profile_data);
  }

  if (dynarray_length(compile_ctx.repts) > 0) {
    int saved_line = compile_ctx.node->line;

    rept_ctx_t *unterm_rept_ctx = (rept_ctx_t *)dfirst(dynarray_last_cell(compile_ctx.repts));

    compile_ctx.node->line = unterm_rept_ctx->rept_node_line;
    report_error_nopos(&compile_ctx, "unterminated REPT block");
    compile_ctx.node->line = saved_line;
  }

  if (dynarray_length(compile_ctx.conditions) > 0) {
    int saved_line = compile_ctx.node->line;

    condition_ctx_t *unterm_cond_ctx = (condition_ctx_t *)dfirst(dynarray_last_cell(compile_ctx.conditions));

    compile_ctx.node->line = unterm_cond_ctx->if_node_line;
    report_error_nopos(&compile_ctx, "unterminated IF block");
    compile_ctx.node->line = saved_line;
  }


  // ==================================================================================================
  // 2nd pass: patch code with unresolved constants values as soon as symtab is fully populated now
  // ==================================================================================================

  foreach(dc, compile_ctx.patches) {
    patch_t *patch = (patch_t *)dfirst(dc);

    compile_ctx.lookup_rept_suffix = patch->rept_suffix;
    parse_node *resolved_node = expr_eval(&compile_ctx, patch->node);
    compile_ctx.lookup_rept_suffix = NULL;

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

  uint32_t dest_size = render_finish(&compile_ctx, dest_buf);

  foreach (dc, compile_ctx.sections) {
    section_ctx_t *section = (section_ctx_t *)dfirst(dc);

    xfree(section->name);
    buffer_free(section->content);
    xfree(section);
  }

  return dest_size;
}

void register_fwd_lookup(compile_ctx_t *ctx,
                          parse_node *unresolved_node,
                          uint32_t pos,
                          int nbytes,
                          bool relative,
                          uint32_t instr_pc)
{
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
  patch->rept_suffix = NULL;

  if (dynarray_length(ctx->repts) > 0) {
    dynarray_cell *dc;

    foreach(dc, ctx->repts) {
      rept_ctx_t *rept_ctx = (rept_ctx_t *)dfirst(dc);

      if (patch->rept_suffix == NULL) {
        patch->rept_suffix = bsprintf("%d", rept_ctx->counter);
      } else {
        char *tmp = patch->rept_suffix;
        patch->rept_suffix = bsprintf("%s#%d", patch->rept_suffix, rept_ctx->counter);
        xfree(tmp);
      }
    }
  }

  ctx->patches = dynarray_append_ptr(ctx->patches, patch);
}

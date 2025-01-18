#include <assert.h>
#include <ctype.h>

#include "common/buffer.h"
#include "common/filesystem.h"
#include "common/hashmap.h"
#include "common/mmgr.h"
#include "common/dynarray.h"
#include "rcc/rcc.h"
#include "rcc/parser.h"
#include "rcc/parse_nodes.h"

#include "rc_parser.tab.h"

#undef YY_DECL
#define YY_DECL int rc_lex (yyscan_t yyscanner, rcc_ctx_t *ctx)
#include "rc_lexer.yy.h"

extern int rc_lex (yyscan_t yyscanner, rcc_ctx_t *ctx);

const char *keywords[] = {
  "assert", "break", "case", "i8", "i16", "u8", "u16", "continue", "default",
  "do", "else", "for", "goto", "if", "inline", "return", "sizeof", "static",
  "struct", "switch", "void", "volatile", "while", "#include", "#define",
  "#ifdef", "#ifndef", "#else", "#endif", "#error", "#warning", "#line", "#file",
};

const char *operators[] = {
  ">>=", "<<=", "+=", "-=", "*=", "/=", "%=", "&=", "^=", "|=", ">>", "<<", "++",
  "--", "->", "&&", "||", "<=", ">=", "==", "!=", ";", "{", "}", ",", ":", "=",
  "(", ")", "[", "]", ".", "&", "!", "~", "-", "+", "*", "/", "%", "<", ">", "^",
  "|", "?",
};

bool is_keyword(const char *id) {
  for (int i = 0; i < sizeof(keywords) / sizeof(keywords[0]); i++) {
    if (strcmp(id, keywords[i]) == 0)
      return true;
  }

  return false;
}

bool is_operator(const char *id) {
  for (int i = 0; i < sizeof(operators) / sizeof(operators[0]); i++) {
    if (strcmp(id, operators[i]) == 0)
      return true;
  }

  return false;
}

bool is_identifier(const char *id) {
  if (id == NULL || *id == '\0')
    return false;

  if (!isalpha(*id) && *id != '_')
    return false;

  for (int i = 1; i < strlen(id); i++)
    if (!isalnum(id[i]) && id[i] != '_')
      return false;

  return true;
}

// skip bounding double quotes if any
// or return NULL if there aren't.
// returns xmalloc-ed string.
char *pure_string_literal(char *lit) {
  if (!lit)
    return NULL;

  char *last_ch = lit + strlen(lit) - 1;

  while(*last_ch == '\n')
    last_ch--;

  if ((lit[0] != '"') || (*last_ch != '"'))
    return NULL;

  char *result = xstrdup(lit + 1);
  result[last_ch - lit - 1] = '\0';
  return result;
}

char *get_current_token(yyscan_t scanner) {
  char *ptext = rc_get_text(scanner);

  if (!ptext)
    return NULL;

  int len = rc_get_leng(scanner);
  char *result = xmalloc(len + 1);
  for (int i = 0; i < len; i++)
    result[i] = ptext[i];

  char *pend = result + len - 1;
  while (*pend == '\n' && pend != result) {
    *pend = '\0';
    pend--;
  }

  return result;
}

static void append_position(buffer *dest, const char *filename, int line_num) {
  buffer_append(dest, "#line \"%s\" %d\n", filename, line_num);
}

static void append_token(buffer *dest, bool *skipped, bool *first_on_line, const char *filename, int line_num, char *text) {
  // if there were tokens consuming while scan, append #line directive for correct diagnostics
  // for next processing stages
  if (*skipped) {
    append_position(dest, filename, line_num);
    *skipped = false;
  }

  // separate tokens by whitespace to preserve 'tokenized' stream for next processing stages
  if (*first_on_line)
    *first_on_line = false;
  else
    buffer_append_char(dest, ' ');

  buffer_append_string(dest, text);
}

static void scan_source(rcc_ctx_t *ctx, buffer *dest, const char *filename, const char *source) {
  yyscan_t scanner;
  struct yy_buffer_state *bstate;
  struct scanner_state sstate = {0};
  bool first_on_line = true;

  append_position(dest, filename, 1);

  char *fullpath = fs_abs_path((char *)filename, ctx->includeopts);
  assert(fullpath);

  // mark this file as already preprocessed
  hashmap_search(ctx->pp_files, fullpath, HASHMAP_INSERT, XMMGR_DUMMY_PTR);

  sstate.line_num = 1;
  sstate.skipped = false;
  sstate.nl_from_scanner = true;

  rc_lex_init(&scanner);
  rc_set_extra(&sstate, scanner);
  bstate = rc__scan_string(source, scanner);

  int token = rc_lex(scanner, ctx);
  while (token) {
    // remember line num for the first token of directive (may be advanced during further processing)
    int line_num = sstate.line_num;

    // process only conditional directives according current conditional state
    #define COND_STATE_TRUE      1  // conditional was true, so we should process tokens as usual
    #define COND_STATE_FALSE     2  // conditional was false, so we shouldn't process tokens; it can inverted by #else
    #define COND_STATE_DISABLED  3  // parent's conditional was false, so we'll false too: don't process tokens; it even can't be inverted by #else

    switch (token) {
      case PP_IFDEF:
      case PP_IFNDEF: {
        int next_token = rc_lex(scanner, ctx);
        int cond_state = COND_STATE_TRUE;

        if (next_token != ID)
          generic_report_error(filename, line_num, "#ifdef: expected constant name");

        if (dynarray_length(ctx->pp_cond_stack) > 0) {
          // if there is a parent block and it's condition is negative,
          // then disable current block unconditionally
          dynarray_cell *top_cond_cell = dynarray_last_cell(ctx->pp_cond_stack);
          if (top_cond_cell->int_value != COND_STATE_TRUE)
            cond_state = COND_STATE_DISABLED;
        } else {
          // if parent block's condition is positive or we're at the first level
          // of conditionals, set conditional state according directive expression
          char *id = get_current_token(scanner);
          void *defined = hashmap_search(ctx->constants, id, HASHMAP_FIND, NULL);
          xfree(id);

          if ((token == PP_IFDEF) == (defined != NULL))
            cond_state = COND_STATE_TRUE;
          else
            cond_state = COND_STATE_FALSE;
        }

        // push conditional state to the stack
        ctx->pp_cond_stack = dynarray_append_int(ctx->pp_cond_stack, cond_state);

        sstate.skipped = true;

        break;
      }

      case PP_ELSE: {
        if (dynarray_length(ctx->pp_cond_stack) == 0)
          generic_report_error(filename, line_num, "#else without #ifdef/#ifndef found");

        dynarray_cell *top_cond_cell = dynarray_last_cell(ctx->pp_cond_stack);

        // don't invert current conditional state if it's in disabled state
        // (i.e. blocked by parent condition)
        if (top_cond_cell->int_value != COND_STATE_DISABLED) {
          top_cond_cell->int_value = (top_cond_cell->int_value == COND_STATE_TRUE) ? COND_STATE_FALSE : COND_STATE_TRUE;
        }

        sstate.skipped = true;
        break;
      }

      case PP_ENDIF: {
        if (dynarray_length(ctx->pp_cond_stack) == 0)
          generic_report_error(filename, line_num, "#endif without #ifdef/#ifndef found");

        // #endif removes current conditional state, even if it in 'disabled' state
        ctx->pp_cond_stack->length--;
        if (ctx->pp_cond_stack->length == 0) {
          dynarray_free(ctx->pp_cond_stack);
          ctx->pp_cond_stack = NULL;
        }

        sstate.skipped = true;
        break;
      }

      default:
        break;
    }

    // if current condition is not positive (i.e. false or disabled),
    // skip this token and consume the next one
    if (dynarray_length(ctx->pp_cond_stack) > 0)
    {
      dynarray_cell *top_cond_cell = dynarray_last_cell(ctx->pp_cond_stack);
      if (top_cond_cell->int_value != COND_STATE_TRUE) {
        token = rc_lex(scanner, ctx);
        continue;
      }
    }

    switch (token) {
      // now skip conditional tokens
      case PP_IFDEF:
      case PP_IFNDEF:
      case PP_ELSE:
      case PP_ENDIF:
        break;

      case PP_INCLUDE: {
        int next_token = rc_lex(scanner, ctx);
        if (next_token != STRING_LITERAL)
          generic_report_error(filename, line_num, "unexpected token after #include: %s", get_current_token(scanner));

        char *inc_filename = pure_string_literal(get_current_token(scanner));
        if (!inc_filename)
          generic_report_error(filename, line_num, "unexpected string literal for #include: %s", get_current_token(scanner));

        sstate.skipped = true;

        char *path = fs_abs_path(inc_filename, ctx->includeopts);
        if (path == NULL)
          generic_report_error(filename, line_num, "file not found: %s", inc_filename);

        void *already_processed = hashmap_search(ctx->pp_files, path, HASHMAP_FIND, NULL);
        if (!already_processed) {
          char *source = read_file(path);
          scan_source(ctx, dest, path, source);
          xfree(source);
        }

        xfree(inc_filename);

        break;
      }

      case PP_DEFINE: {
        int next_token = rc_lex(scanner, ctx);
        char *key;
        char *value;

        if (next_token != ID)
          generic_report_error(filename, line_num, "#define: expected constant name");

        key = get_current_token(scanner);

        next_token = rc_lex(scanner, ctx);
        if (next_token == '\n')
          value = xstrdup("");
        else
          value = get_current_token(scanner);

        hashmap_search(ctx->constants, key, HASHMAP_INSERT, value);
        sstate.skipped = true;

        xfree(key);

        break;
      }

      case PP_ERROR: {
        int next_token = rc_lex(scanner, ctx);
        if (next_token != STRING_LITERAL)
          generic_report_error(filename, line_num, "unexpected token after #warning: %s", get_current_token(scanner));

        generic_report_error(filename, line_num, "%s", get_current_token(scanner));
        break;
      }

      case PP_WARNING: {
        int next_token = rc_lex(scanner, ctx);
        if (next_token != STRING_LITERAL)
          generic_report_error(filename, line_num, "unexpected token after #warning: %s", get_current_token(scanner));

        generic_report_warning(filename, line_num, "%s", get_current_token(scanner));
        sstate.skipped = true;

        break;
      }

      case '\n':
        // printf("found nl on %d\n", line_num);
        first_on_line = true;
        buffer_append_char(dest, '\n');
        break;

      case ID: {
        char *key = get_current_token(scanner);
        char *value = (char *)hashmap_search(ctx->constants, key, HASHMAP_FIND, NULL);

        if (value) {
          // found constant with that name: replace to its value
          append_token(dest, &sstate.skipped, &first_on_line, filename, line_num, value);
        } else {
          append_token(dest, &sstate.skipped, &first_on_line, filename, line_num, key);
        }

        xfree(key);

        break;
      }

      default: {
        char *str;

        if (token == STRING_LITERAL)
          str = bsprintf("\"%s\"", sstate.string_literal);
        else
          str = get_current_token(scanner);
        append_token(dest, &sstate.skipped, &first_on_line, filename, line_num, str);
        xfree(str);
        break;
      }
    }

    token = rc_lex(scanner, ctx);
  }

  rc__delete_buffer(bstate, scanner);
  rc_lex_destroy(scanner);
}

extern int get_actual_position(rcc_ctx_t *ctx, int scanner_pos, char **filename) {
  int line_num = 1;
  bool found_scanner_pos = false;

  char *p = ctx->pp_output_str;
  while (*p) {
    if (line_num == scanner_pos - 1) {
      found_scanner_pos = true;
      break;
    }

    // move to desired line
    if (p[0] == '\n') {
      line_num++;
    }

    p++;
  }

  if (!found_scanner_pos) {
    if (filename)
      *filename = NULL;
    return -1;
  }

  int distance = 0;

  // rewind until #line found
  while(p > ctx->pp_output_str) {
    p--;

    if (*p == '\n')
      distance++;

    const char *pos_pattern = "\n#line ";
    int len = strlen(pos_pattern);
    if (memcmp(p, pos_pattern, len) == 0) {
      p += len;
      break;
    }
  }

  // we're at file begin, nothing to adjust
  if (p == ctx->pp_output_str)
    return scanner_pos;

  // #line found: skip filename and move to actual line num
  assert(*p == '"');
  p++;
  char *filename_beg = p;
  char *filename_end;
  char *num_beg;
  char *num_end;

  while (*p++ != '"');
  filename_end = p - 2;

  // skip whitespace between filename and num
  while (*p++ == ' ');
  num_beg = p - 1;

  while (*p++ != '\n');
  num_end = p - 2;

  char *file_str = xmalloc(filename_end - filename_beg + 2);
  memcpy (file_str, filename_beg, (filename_end - filename_beg + 1));
  file_str[filename_end - filename_beg + 1] = '\0';

  char *num_str = xmalloc(num_end - num_beg + 2);
  memcpy (num_str, num_beg, (num_end - num_beg + 1));
  num_str[num_end - num_beg + 1] = '\0';

  int pos = atoi(num_str);
  if (filename)
    *filename = file_str;

  return pos + distance - 1;
}

char *do_preproc(rcc_ctx_t *ctx, const char *source) {
  buffer *dest = buffer_init_ex("scan_dest");
  char *result;

  scan_source(ctx, dest, ctx->in_filename, source);

  result = buffer_dup(dest);
  buffer_free(dest);
  return result;
}

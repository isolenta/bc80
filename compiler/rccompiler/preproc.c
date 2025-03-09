#include <assert.h>
#include <ctype.h>

#include "common/buffer.h"
#include "common/filesystem.h"
#include "common/hashmap.h"
#include "common/mmgr.h"
#include "common/dynarray.h"
#include "rccompiler/rcc.h"
#include "rccompiler/parser.h"
#include "rccompiler/parse_nodes.h"

#include "rc_parser.tab.h"

#undef YY_DECL
#define YY_DECL int rc_lex (yyscan_t yyscanner, rcc_ctx_t *ctx)
#include "rc_lexer.yy.h"

extern int rc_lex (yyscan_t yyscanner, rcc_ctx_t *ctx);

const char *keywords[] = {
  "static_assert", "break", "case", "i8", "i16", "u8", "u16", "continue", "default",
  "do", "else", "for", "goto", "if", "inline", "return", "sizeof", "static",
  "struct", "switch", "void", "volatile", "while", "#include", "#define",
  "#ifdef", "#ifndef", "#else", "#endif", "#error", "#warning", "#line",
  "bool", "false", "true",
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

static void check_alter_position(buffer *dest, bool *skipped, const char *filename, int line_num) {
  if (*skipped) {
    append_position(dest, filename, line_num);
    *skipped = false;
  }
}

static void scan_source(rcc_ctx_t *ctx, buffer *dest, const char *filename, const char *source) {
  yyscan_t scanner;
  struct yy_buffer_state *bstate;
  struct scanner_state sstate = {0};

  append_position(dest, filename, 1);

  char *fullpath = fs_abs_path((char *)filename, ctx->includeopts);
  assert(fullpath);

  // mark this file as already preprocessed
  hashmap_set(ctx->pp_files, fullpath, XMMGR_DUMMY_PTR);

  ctx->current_position.filename = (char *)filename;
  sstate.rcc_ctx = ctx;
  sstate.line_num = 1;
  sstate.pos_num = 1;
  sstate.skipped = false;
  sstate.scan_standalone = true;
  sstate.source_ptr = (char *)source;

  rc_lex_init(&scanner);
  rc_set_extra(&sstate, scanner);
  bstate = rc__scan_string(source, scanner);

  int token = rc_lex(scanner, ctx);
  while (token) {
    // remember line num for the first token of directive (may be advanced during further processing)
    int line_num = sstate.line_num;
    int pos_num = sstate.pos_num;

    if (token != '\n' && dest->len > 0 && dest->data[dest->len - 1] == '\n') {
      check_alter_position(dest, &sstate.skipped, filename, line_num);
    }

    // process only conditional directives according current conditional state
    #define COND_STATE_TRUE      1  // conditional was true, so we should process tokens as usual
    #define COND_STATE_FALSE     2  // conditional was false, so we shouldn't process tokens; it can inverted by #else
    #define COND_STATE_DISABLED  3  // parent's conditional was false, so we'll false too: don't process tokens; it even can't be inverted by #else

    switch (token) {
      case PP_IFDEF:
      case PP_IFNDEF: {
        int next_token;

        // consume whitespace between tokens
        next_token = rc_lex(scanner, ctx);
        if (next_token != WHITESPACE)
          report_error(ctx, "#%s: expected whitespace",
            ((token == PP_IFDEF) ? "ifdef" : "ifndef"));

        int cond_state = COND_STATE_TRUE;

        next_token = rc_lex(scanner, ctx);
        if (next_token != ID)
          report_error(ctx, "#ifdef: expected constant name");

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
          void *defined = hashmap_get(ctx->constants, id);
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
          report_error(ctx, "#else without #ifdef/#ifndef found");

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
          report_error(ctx, "#endif without #ifdef/#ifndef found");

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
        sstate.skipped = false;
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
        int next_token;

        // consume whitespace between tokens
        next_token = rc_lex(scanner, ctx);
        if (next_token != WHITESPACE)
          report_error(ctx, "#include: expected whitespace");

        next_token = rc_lex(scanner, ctx);
        if (next_token != STRING_LITERAL)
          report_error(ctx, "#include: expected string literal");

        char *inc_filename = sstate.string_literal;
        if (!inc_filename)
          report_error(ctx, "#include: unexpected string literal %s", get_current_token(scanner));

        sstate.skipped = true;

        char *path = fs_abs_path(inc_filename, ctx->includeopts);
        if (path == NULL)
          report_error(ctx, "file not found: %s", inc_filename);

        void *already_processed = hashmap_get(ctx->pp_files, path);
        if (!already_processed) {
          char *source = read_file(path);
          ctx->current_position.filename = path;
          scan_source(ctx, dest, path, source);
          xfree(source);
        }

        xfree(inc_filename);

        break;
      }

      case PP_DEFINE: {
        int next_token;
        char *key;
        char *value;
        bool multiline_define = false;

        // consume whitespace between tokens
        next_token = rc_lex(scanner, ctx);
        if (next_token != WHITESPACE)
          report_error(ctx, "#define: expected whitespace");

        next_token = rc_lex(scanner, ctx);

        if (next_token != ID)
          report_error(ctx, "#define: expected constant name");

        key = get_current_token(scanner);

        next_token = rc_lex(scanner, ctx);
        if (next_token == '\n') {
          value = xstrdup("");
        } else {
          // #define with value: must be whitespace
          if (next_token != WHITESPACE)
            report_error(ctx, "#define: expected whitespace");

          // get all tokens until newline
          buffer *value_buf = buffer_init();
          for (;;) {
            next_token = rc_lex(scanner, ctx);

            if (next_token == '\n') {
              break;
            } else if (next_token == BACKSLASH) {
              sstate.line_num++;
              multiline_define = true;
              continue;
            } else if (next_token == WHITESPACE) {
              buffer_append_string(value_buf, sstate.whitespace_str);
              xfree(sstate.whitespace_str);
            } else if (next_token == STRING_LITERAL) {
              buffer_append_string(value_buf, sstate.string_literal);
            } else if (next_token == INT_LITERAL) {
              buffer_append(value_buf, "%d", sstate.int_literal);
            } else {
              buffer_append_string(value_buf, get_current_token(scanner));
            }
          }

          value = buffer_dup(value_buf);
          buffer_free(value_buf);
        }

        hashmap_set(ctx->constants, key, value);
        sstate.skipped = multiline_define;

        xfree(key);

        break;
      }

      case PP_ERROR: {
        int next_token;

        // consume whitespace between tokens
        next_token = rc_lex(scanner, ctx);
        if (next_token != WHITESPACE)
          report_error(ctx, "#error: expected whitespace");

        next_token = rc_lex(scanner, ctx);
        if (next_token != STRING_LITERAL)
          report_error(ctx, "#error: expected string literal");

        generic_report_error(ERROR_OUT_LOC | ERROR_OUT_LINE,
          ctx->current_position.filename, ctx->current_position.line, 0,
          "%s", sstate.string_literal);
        break;
      }

      case PP_WARNING: {
        int next_token;

        // consume whitespace between tokens
        next_token = rc_lex(scanner, ctx);
        if (next_token != WHITESPACE)
          report_error(ctx, "#warning: expected whitespace");

        next_token = rc_lex(scanner, ctx);
        if (next_token != STRING_LITERAL)
          report_error(ctx, "#warning: expected string literal");

        generic_report_warning(ERROR_OUT_LOC | ERROR_OUT_LINE,
          ctx->current_position.filename, ctx->current_position.line, 0,
          "%s", sstate.string_literal);

        break;
      }

      case '\n':
        buffer_append_char(dest, '\n');
        break;

      case ID: {
        char *key = get_current_token(scanner);
        char *value = (char *)hashmap_get(ctx->constants, key);

        // check_alter_position(dest, &sstate.skipped, filename, line_num);

        if (value) {
          // found constant with that name: replace to its value
          buffer_append_string(dest, value);
        } else {
          buffer_append_string(dest, key);
        }

        xfree(key);

        break;
      }

      case STRING_LITERAL:
        buffer_append_char(dest, '"');
        buffer_append_string(dest, sstate.string_literal);
        buffer_append_char(dest, '"');
        xfree(sstate.string_literal);
        break;

      case INT_LITERAL:
        buffer_append(dest, "%d", sstate.int_literal);
        break;

      case END_OF_COMMENT:
        // replace multiline comment to th same number of empty lines
        for (int i = 0; i < sstate.num_comment_lines; i++)
          buffer_append_char(dest, '\n');
        break;

      case WHITESPACE:
        // for preprocessed output preserve the same whitespaces (if any) between tokens
        buffer_append_string(dest, sstate.whitespace_str);
        xfree(sstate.whitespace_str);
        break;

      default: {
        char *str = get_current_token(scanner);
        // check_alter_position(dest, &sstate.skipped, filename, line_num);
        buffer_append_string(dest, str);
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
    if (line_num == scanner_pos) {
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
    // EOF: rewind to last non-empty line
    while(p > ctx->pp_output_str) {
      p--;
      if (*p == '\n')
        line_num--;
      else
        break;
    }
  }

  int distance = 0;

  // rewind until #line found
  const char *pos_pattern = "#line ";
  int len = strlen(pos_pattern);
  while(p > ctx->pp_output_str) {
    p--;

    if (*p == '\n')
      distance++;

    if ((memcmp(p, pos_pattern, len) == 0) &&
          (p == ctx->pp_output_str || *(p - 1) == '\n'))
    {
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

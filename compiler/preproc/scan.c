#include <stdio.h>
#include <stdbool.h>
#include <stdint.h>
#include <string.h>
#include <stdlib.h>
#include <stdarg.h>
#include <unistd.h>
#include <ctype.h>
#include <time.h>

#include "filesystem.h"
#include "mmgr.h"
#include "buffer.h"
#include "preproc.h"
#include "error.h"

char *read_file_content(preproc_ctx_t *ctx, char *filename) {
  char *source = NULL;
  size_t filesize, sret;
  FILE *fin = NULL;

  if (!fs_file_exists(filename))
    report_error(ctx, "file is not exist: %s\n", filename);

  filesize = fs_file_size(filename);
  fin = fopen(filename, "r");
  if (!fin)
    report_error(ctx, "unable to open file %s\n", filename);

  source = (char *)xmalloc(filesize + 2);

  sret = fread(source, filesize, 1, fin);
  if (sret != 1) {
    fclose(fin);
    report_error(ctx, "file reading error %s\n", filename);
  }

  source[filesize] = '\0';
  source[filesize + 1] = '\0';

  fclose(fin);

  return source;
}

// true if next token is lexeme, false otherwise
static bool next_token(char *pstr, int *offset, int *len) {
  char *p = pstr;

  *offset = 0;

  // skip whitespaces
  while (*p) {
    if (!isblank(*p))
      break;
    p++;
    (*offset)++;
  }

  // check if next token is non-lexeme
  if (!(isalnum(*p) || (*p == '_'))) {
    // non-lexeme token always has one symbol
    *len = 1;
    return false;
  }

  // scan until lexeme ends
  *len = 0;
  while (*p) {
    if (!(isalnum(*p) || (*p == '_')))
      break;

    (*len)++;
    p++;
  }

  return true;
}

static bool is_start_of_line(char *pos, const char *origin) {
  pos--;
  while ((pos >= origin) && (*pos != '\n')) {
    if ((*pos != ' ') && (*pos != '\t'))
      return false;
    pos--;
  }
  return true;
}

static void trim_trailing_spaces(buffer *buf) {
  while (((buf->data[buf->len - 1] == ' ') || (buf->data[buf->len - 1] == '\t')) && buf->len > 0)
    buf->len--;
}

static char *extract_until_eol(char *pos, int *current_line) {
  buffer *buf = buffer_init();
  char *result;
  char *p = pos;

  while (*p) {
    // consume escaped newlines
    if ((*p == '\\') && (p > pos) && (*(p - 1) != '\\') && (*(p + 1) == '\n')) {
      buffer_append_char(buf, ' ');
      p += 2;
      *current_line += 1;
      continue;
    }

    if (*p == '\n')
      break;

    buffer_append_char(buf, *p++);
  }

  result = buffer_dup(buf);
  buffer_free(buf);
  return result;
}

static char *get_include_path(preproc_ctx_t *ctx, char *argsline, bool *sysroot) {
  int token_len, token_off;
  char *p = argsline;
  bool term;
  char *path;

  bool is_lexeme = next_token(p, &token_off, &token_len);
  if (is_lexeme)
    report_error(ctx, "unexpected token in include path\n");

  p += token_off;

  if (*p == '"')
    *sysroot = false;
  else if (*p == '<')
    *sysroot = true;
  else
    report_error(ctx, "unexpected token in include path\n");

  p++;

  term = false;
  buffer *incpath = buffer_init();

  while (1) {
    is_lexeme = next_token(p, &token_off, &token_len);
    p += token_off;

    if (!is_lexeme && (*p == '\n'))
      break;

    if (!is_lexeme && *sysroot && (*p == '>')) {
      term = true;
      break;
    }

    if (!is_lexeme && !(*sysroot) && (*p == '"')) {
      term = true;
      break;
    }

    buffer_append_binary(incpath, p, token_len);
    p += token_len;
  }

  if (!term)
    report_error(ctx, "unterminated include path\n");

  buffer_append_char(incpath, '\0');
  path = buffer_dup(incpath);
  buffer_free(incpath);

  return path;
}

static void process_define(preproc_ctx_t *ctx, char *argsline) {
  int token_off, token_len;
  char *name;
  bool is_lexeme = next_token(argsline, &token_off, &token_len);
  if (!is_lexeme)
    report_error(ctx, "unexpected symbol after #define\n");

  name = xmalloc(token_len + 1);
  strncpy(name, argsline, token_len);
  name[token_len] = 0;

  if (isblank(argsline[token_len])) {
    // objects-like macros (i.e. constant)

    char *parg = argsline + token_len;
    while(*parg && isblank(*parg))
      parg++;

    hashmap_search(ctx->constants, name, HASHMAP_INSERT, xstrdup(parg));
  } else if (argsline[token_len] == '(') {
    // function-like macros
    printf("func-like macros: %s => ???\n", name);
  }

  xfree(name);
}

char *scan(preproc_ctx_t *ctx, const char *source, const char *filename) {
  buffer *dest = buffer_init();
  char *result;
  int line = 1;
  bool append_line_directive = false;

  int *num_procs = hashmap_search(ctx->processed_files, fs_abs_path((char *)filename, NULL), HASHMAP_FIND, NULL);
  if (!num_procs) {
    num_procs = (int *)xmalloc(sizeof(int));
    *num_procs = 1;
    hashmap_search(ctx->processed_files, fs_abs_path((char *)filename, NULL), HASHMAP_INSERT, num_procs);
  } else {
    (*num_procs)++;
  }

  // add #line directive explicitly at the beginning of each processing
  buffer_append(dest, "#line 1 \"%s\"\n", filename);

#define CHAR_IS_ESCAPED(s) (((s) > source) && (*((s) - 1) == '\\') && ((((s) - 1) == source) || (*((s) - 2) != '\\')))

  for (char *pstr = (char *)source; *pstr; pstr++) {
    int token_off, token_len;

    if (*pstr == '\n')
      line++;

    // for single line comment skip all until end of line
    if (strstr(pstr, "//") == pstr) {
      while (*pstr) {
        if (*pstr == '\n' && !CHAR_IS_ESCAPED(pstr)) {
          line++;
          break;
        }
        pstr++;
      }

      // remove trailing spaces (before comment begin)
      trim_trailing_spaces(dest);
    }

    // multiline comment
    if (strstr(pstr, "/*") == pstr) {
      while (*pstr) {
        if (*pstr == '\n' && !CHAR_IS_ESCAPED(pstr)) {
          line++;
        }

        if (strstr(pstr, "*/") == pstr) {
          pstr += 2;
          break;
        }

        pstr++;
      }

      // remove trailing spaces (before comment begin)
      trim_trailing_spaces(dest);

      if (*pstr == '\n')
        line++;

      append_line_directive = true;
    }

    // character literal
    if (*pstr == '\'' && !CHAR_IS_ESCAPED(pstr)) {
      buffer_append_char(dest, *pstr);
      pstr++;
      while (*pstr) {
        if (*pstr == '\n' && !CHAR_IS_ESCAPED(pstr))
          report_error(ctx, "unterminated character at line %d\n", line);

        if (*pstr == '\'' && !CHAR_IS_ESCAPED(pstr))
          break;

        buffer_append_char(dest, *pstr);
        pstr++;
      }
    }

    // string literal
    if (*pstr == '"' && !CHAR_IS_ESCAPED(pstr)) {
      buffer_append_char(dest, *pstr);
      pstr++;
      while (*pstr) {
        if (*pstr == '\n' && !CHAR_IS_ESCAPED(pstr))
          report_error(ctx, "unterminated string at line %d\n", line);

        if (*pstr == '"' && !CHAR_IS_ESCAPED(pstr))
          break;

        buffer_append_char(dest, *pstr);
        pstr++;
      }
    }

    // merge lines with escaped newline character
    if (*pstr == '\n' && CHAR_IS_ESCAPED(pstr)) {
      pstr++;
      dest->len--;
      append_line_directive = true;
    }

    // directive: must be a first token in line
    if (*pstr == '#' && !CHAR_IS_ESCAPED(pstr) && is_start_of_line(pstr, source)) {
      int token_off = 0, token_len = 0;

      pstr++;
      if (!next_token(pstr, &token_off, &token_len))
        report_error(ctx, "unexpected symbol in the directive: %c\n", *(pstr + token_off));

      pstr += token_off;

      if (strncasecmp(pstr, "include", token_len) == 0) {
        bool sysroot;
        pstr += token_len;
        char *path = get_include_path(ctx, pstr, &sysroot);

        // seek to the end of line
        while (*pstr && (*pstr != '\n'))
          pstr++;

        line++;

        // TODO: handle sysroot and include paths
        char *incsource = read_file_content(ctx, path);
        char *incresult = scan(ctx, incsource, path);

        trim_trailing_spaces(dest);
        buffer_append(dest, "%s", incresult);
        append_line_directive = true;

        xfree(incresult);
        xfree(incsource);
        xfree(path);
      } else if (strncasecmp(pstr, "define", token_len) == 0) {
        pstr += sizeof("define");
        char *text = extract_until_eol(pstr, &line);
        pstr += strlen(text);
        process_define(ctx, text);
        xfree(text);
      } else if (strncasecmp(pstr, "warning", token_len) == 0) {
        pstr += sizeof("warning");
        char *text = extract_until_eol(pstr, &line);
        pstr += strlen(text);
        report_warning(ctx, text);
        xfree(text);
      } else if (strncasecmp(pstr, "error", token_len) == 0) {
        pstr += sizeof("error");
        char *text = extract_until_eol(pstr, &line);
        pstr += strlen(text);
        report_error(ctx, text);
        xfree(text); // not reachable
      } else if (strncasecmp(pstr, "pragma", token_len) == 0) {
        pstr += sizeof("pragma");

        if (next_token(pstr, &token_off, &token_len)) {
          pstr += token_off;
          if (strncasecmp(pstr, "once", token_len) == 0) {
            int *num_procs = hashmap_search(ctx->processed_files, fs_abs_path((char *)filename, NULL), HASHMAP_FIND, NULL);
            if (*num_procs > 1)
              return xstrdup("");
          } else {
            // leave unknown pragmas for compiler
            char *text = extract_until_eol(pstr, &line);
            pstr += strlen(text);
            buffer_append(dest, "#pragma %s\n", text);
            xfree(text);
          }
        }
      } else {
        char *name = xmalloc(token_len + 1);
        memcpy(name, pstr, token_len);
        name[token_len] = 0;
        pstr += token_len;
        report_warning(ctx, "unknown directive #%s", name);
      }
    }

    // constant replacement
    if (next_token(pstr, &token_off, &token_len)) {
      char *lexeme, *replacement;

      for (int i = 0; i < token_off; i++)
        buffer_append_char(dest, *pstr++);

      lexeme = xmalloc(token_len + 1);
      memcpy(lexeme, pstr, token_len);
      lexeme[token_len] = 0;

      replacement = NULL;

      if (strcasecmp(lexeme, "__line__") == 0)
        replacement = bsprintf("%u", line);
      else if (strcasecmp(lexeme, "__file__") == 0)
        replacement = xstrdup(filename);
      else if (strcasecmp(lexeme, "__counter__") == 0)
        replacement = bsprintf("%u", ctx->unique_counter++);
      else if (strcasecmp(lexeme, "__date__") == 0) {
        char tmp[100];
        time_t now = time(NULL);
        strftime(tmp, sizeof(tmp) - 1, "%d/%m/%Y", localtime(&now));
        replacement = xstrdup(tmp);
      } else if (strcasecmp(lexeme, "__time__") == 0) {
        char tmp[100];
        time_t now = time(NULL);
        strftime(tmp, sizeof(tmp) - 1, "%H:%M:%S", localtime(&now));
        replacement = xstrdup(tmp);
      } else if (strcasecmp(lexeme, "__timestamp__") == 0) {
        char tmp[100];
        time_t now = time(NULL);
        strftime(tmp, sizeof(tmp) - 1, "%d/%m/%Y %H:%M:%S", localtime(&now));
        replacement = xstrdup(tmp);
      } else
        replacement = xstrdup(hashmap_search(ctx->constants, lexeme, HASHMAP_FIND, NULL));

      if (replacement) {
        buffer_append_binary(dest, replacement, strlen(replacement));
        xfree(replacement);
      } else {
        buffer_append_binary(dest, pstr, token_len);
      }

      pstr += token_len - 1;

      xfree(lexeme);
    } else {
      buffer_append_char(dest, *pstr);
    }

    // don't consume token delimiters
    if (isblank(*pstr))
      buffer_append_char(dest, *pstr);

    // add #line if line ordering was broken during substitutions or replacements
    if ((dest->data[dest->len - 1] == '\n') && append_line_directive) {
      buffer_append(dest, "#line %d \"%s\"\n", line, filename);
      append_line_directive = false;
    }
  }

  buffer_append_char(dest, '\0');

  result = buffer_dup(dest);
  buffer_free(dest);
  return result;
}

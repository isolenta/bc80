#include <ctype.h>
#include <stdio.h>

#include "asm/libasm.h"
#include "asm/parse.h"
#include "common/buffer.h"
#include "common/dynarray.h"
#include "common/filesystem.h"
#include "common/mmgr.h"

#include "parser.tab.h"
#include "lexer.yy.h"

parse_node *new_node_macro_holder;
static dynarray *g_includeopts;

int parse_source(struct libasm_as_desc_t *desc, dynarray **statements)
{
  struct yy_buffer_state *buffer;
  yyscan_t scanner;
  int result;
  char *data;
  struct as_scanner_state sstate;

  g_includeopts = desc->includeopts;

  size_t len = strlen(desc->source);
  data = (char *)xmalloc(len + 2);
  strcpy(data, desc->source);

  // add extra NL for correct parsing
  data[len] = '\n';
  data[len + 1] = '\0';

  sstate.line_num = 1;
  sstate.pos_num = 1;

  yylex_init(&scanner);
  yyset_extra(&sstate, scanner);
  buffer = yy_scan_string(data, scanner);
  result = yyparse(scanner, statements, desc);
  yy_delete_buffer(buffer, scanner);
  yylex_destroy(scanner);

  xfree(data);

  return result;
}

int parse_hexnum(struct libasm_as_desc_t *desc, char *text, int len) {
  int result;
  char *tmp, *endptr;

  tmp = text;

  if (text[0] == '$')
    tmp = text + 1;
  else if ((text[0] == '0') && (text[1] == 'x'))
    tmp = text + 2;
  else if (tolower(text[len-1]) == 'h') {
    tmp = xstrdup(text);
    tmp[len-1] = 0;
  }

  result = strtol(tmp, &endptr, 16);
  if (endptr == tmp) {
    report_error_noloc("error parse hexademical integer: %s", text);
  }

  return result;
}

int parse_decnum(struct libasm_as_desc_t *desc, char *text, int len) {
  int result;
  char *endptr;

  result = strtol(text, &endptr, 10);
  if (endptr == text) {
    report_error_noloc("error parse decimal integer: %s", text);
  }

  return result;
}

int parse_binnum(struct libasm_as_desc_t *desc, char *text, int len) {
  char *tmp;

  if (text[0] == '%') {
    tmp = text + 1;
  } else if ((text[0] == '0') && (text[1] == 'b')) {
    tmp = text + 2;
  } else if (tolower(text[len-1]) == 'b') {
    tmp = xstrdup(text);
    tmp[len - 1] = '\0';
  }

  int result;
  char *endptr;

  result = strtol(tmp, &endptr, 2);
  if (endptr == tmp) {
    report_error_noloc("error parse binary integer: %s", text);
  }

  return result;
}

int parse_include(struct libasm_as_desc_t *desc, dynarray **statements, char *filename) {
  int ret = 0;
  size_t file_size, sret;
  FILE *fp = NULL;
  char *path;
  char *source = NULL;
  struct libasm_as_desc_t desc_subtree;

  path = fs_abs_path(filename, g_includeopts);
  if (path == NULL) {
    report_error_noloc("file not found: %s", filename);
    goto out;
  }

  file_size = fs_file_size(path);
  fp = fopen(path, "r");
  if (!fp) {
    report_error_noloc("%s: %s", path, strerror(errno));
    goto out;
  }

  source = (char *)xmalloc(file_size + 1);
  sret = fread(source, file_size, 1, fp);
  if (sret != 1) {
    report_error_noloc("%s: %s", path, strerror(errno));
    goto out;
  }

  source[file_size] = 0;

  memcpy(&desc_subtree, desc, sizeof(struct libasm_as_desc_t));
  desc_subtree.source = source;
  desc_subtree.filename = filename;
  ret = parse_source(&desc_subtree, statements);

out:
  if (source)
    xfree(source);
  if (fp)
    fclose(fp);

  return ret;
}

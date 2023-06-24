#include <stdio.h>
#include <string.h>
#include <errno.h>
#include <ctype.h>
#include <sys/stat.h>
#include <unistd.h>

#include "libasm80.h"
#include "buffer.h"
#include "parse.h"
#include "filesystem.h"
#include "dynarray.h"
#include "parser.tab.h"
#include "lexer.yy.h"

parse_node *new_node_macro_holder;
static dynarray *g_includeopts;

const char *get_parse_node_name(parse_node *node) {
  switch (node->type) {
    case NODE_DEF: {
      return "def";
      break;
    }
    case NODE_END: {
      return "end";
      break;
    }
    case NODE_ORG: {
      return "org";
      break;
    }
    case NODE_INCBIN: {
      return "incbin";
      break;
    }
    case NODE_EQU: {
      return "equ";
      break;
    }
    case NODE_LITERAL: {
      return "literal";
      break;
    }
    case NODE_LABEL: {
      return "label";
      break;
    }
    case NODE_INSTR: {
      return "instruction";
      break;
    }
    case NODE_EXPR: {
      return "expression";
      break;
    }
    case NODE_ID: {
      return "identifier";
      break;
    }
    case NODE_LIST: {
      return "list";
      break;
    }
    default:
      break;
  }
  return "unknown";
}

const char *get_literal_kind(LITERAL *l) {
  switch (l->kind) {
    case INT: {
      return "integer";
      break;
    }
    case STR: {
      return "string";
      break;
    }
    case DOLLAR: {
      return "dollar special";
      break;
    }
    default:
      break;
  }
  return "unknown";
}

void print_node(parse_node *node) {
  switch (node->type) {
    case NODE_DEF: {
      DEF *l = (DEF *)node;
      printf("(%s ",
        (l->kind == DEFKIND_DB ? "DB" :
         l->kind == DEFKIND_DM ? "DM" :
         l->kind == DEFKIND_DW ? "DW" :
         l->kind == DEFKIND_DS ? "DS" :
         "unknown_def"));
      print_node((parse_node *)l->values);
      printf(") ");
      break;
    }
    case NODE_END: {
      printf("(END) ");
      break;
    }
    case NODE_ORG: {
      ORG *l = (ORG *)node;
      printf("(ORG ");
      print_node((parse_node *)l->value);
      printf(") ");
      break;
    }
    case NODE_INCBIN: {
      INCBIN *l = (INCBIN *)node;
      printf("(INCBIN ");
      print_node((parse_node *)l->filename);
      printf(") ");
      break;
    }
    case NODE_EQU: {
      EQU *l = (EQU *)node;
      printf("(EQU ");
      print_node((parse_node *)l->name);
      printf("= ");
      print_node((parse_node *)l->value);
      printf(") ");
      break;
    }
    case NODE_LITERAL: {
      LITERAL *l = (LITERAL *)node;
      printf("(LITERAL %s %s ",
        (l->is_ref ? "ref" : ""),
        (l->kind == INT ? "int" : l->kind == STR ? "str" : l->kind == DOLLAR ? "DOLLAR" : "(unknown_lit)"));
      if (l->kind == INT)
        printf("%d", l->ival);
      else if (l->kind == STR)
        printf("\"%s\"", l->strval);
      printf(") ");
      break;
    }
    case NODE_LABEL: {
      LABEL *l = (LABEL *)node;
      printf("(LABEL ");
      print_node((parse_node *)l->name);
      printf(") ");
      break;
    }
    case NODE_INSTR: {
      INSTR *l = (INSTR *)node;
      printf("(INSTR ");
      print_node((parse_node *)l->name);
      if (l->args) {
        printf("args: ");
        print_node((parse_node *)l->args);
      }
      printf(") ");
      break;
    }
    case NODE_EXPR: {
      EXPR *l = (EXPR *)node;

      printf("(EXPR ");
      switch (l->kind) {
        case SIMPLE:
          printf("simple ");
          break;
        case UNARY_PLUS:
          printf("uplus ");
          break;
        case UNARY_MINUS:
          printf("uminus ");
          break;
        case UNARY_NOT:
          printf("unot ");
          break;
        case BINARY_PLUS:
          printf("bplus ");
          break;
        case BINARY_MINUS:
          printf("bminus ");
          break;
        case BINARY_OR:
          printf("or ");
          break;
        case BINARY_AND:
          printf("and ");
          break;
        case BINARY_DIV:
          printf("div ");
          break;
        case BINARY_MUL:
          printf("mul ");
          break;
        default:
          break;
      }

      if (l->is_ref)
        printf("ref ");

      print_node(l->left);
      printf(" ");
      if (!(l->kind == SIMPLE || l->kind == UNARY_NOT || l->kind == UNARY_PLUS || l->kind == UNARY_MINUS))
        print_node(l->right);

      printf(") ");

      break;
    }
    case NODE_ID: {
      ID *l = (ID *)node;
      printf("(ID %s %s) ", (l->is_ref ? "ref" : ""), l->name);
      break;
    }
    case NODE_LIST: {
      dynarray_cell *dc = NULL;
      LIST *l = (LIST *)node;
      printf("(LIST ");

      if (l->list)
        foreach(dc, l->list) {
          print_node((parse_node *)dfirst(dc));
          printf(" ");
        }

      printf(") ");

      break;
    }
    default:
      break;
  }
}

static void node_to_string_recurse(parse_node *node, buffer *buf) {
  switch (node->type) {
    case NODE_DEF: {
      DEF *l = (DEF *)node;
      buffer_append(buf, "%s ",
        (l->kind == DEFKIND_DB ? "DB" :
         l->kind == DEFKIND_DM ? "DM" :
         l->kind == DEFKIND_DW ? "DW" :
         l->kind == DEFKIND_DS ? "DS" :
         "unknown_def"));
      node_to_string_recurse((parse_node *)l->values, buf);
      break;
    }
    case NODE_ORG: {
      ORG *l = (ORG *)node;
      buffer_append(buf, "ORG ");
      node_to_string_recurse((parse_node *)l->value, buf);
      break;
    }
    case NODE_INCBIN: {
      INCBIN *l = (INCBIN *)node;
      buffer_append(buf, "INCBIN ");
      node_to_string_recurse((parse_node *)l->filename, buf);
      break;
    }
    case NODE_EQU: {
      EQU *l = (EQU *)node;
      node_to_string_recurse((parse_node *)l->name, buf);
      buffer_append(buf, "EQU ");
      node_to_string_recurse((parse_node *)l->value, buf);
      break;
    }
    case NODE_LITERAL: {
      LITERAL *l = (LITERAL *)node;
      if (l->is_ref)
        buffer_append(buf, "(");

      if (l->kind == INT)
        buffer_append(buf, "%d", l->ival);
      else if (l->kind == DOLLAR)
        buffer_append(buf, "$");
      else if (l->kind == STR)
        buffer_append(buf, "'%s'", l->strval);

      if (l->is_ref)
        buffer_append(buf, ")");
      break;
    }
    case NODE_LABEL: {
      LABEL *l = (LABEL *)node;
      node_to_string_recurse((parse_node *)l->name, buf);
      buffer_append(buf, ":");
      break;
    }
    case NODE_INSTR: {
      INSTR *l = (INSTR *)node;
      dynarray_cell *dc = NULL;

      node_to_string_recurse((parse_node *)l->name, buf);
      buffer_append(buf, " ");

      if (l->args) {
        foreach(dc, l->args->list) {
          node_to_string_recurse((parse_node *)dfirst(dc), buf);

          if (dc__state.i < (l->args->list->length - 1))
            buffer_append(buf, ", ");
        }
      }

      break;
    }
    case NODE_EXPR: {
      EXPR *l = (EXPR *)node;

      if (l->is_ref)
        buffer_append(buf, "(");

      switch (l->kind) {
        case SIMPLE:
          node_to_string_recurse(l->left, buf);
          break;
        case UNARY_PLUS:
          buffer_append(buf, "+");
          node_to_string_recurse(l->left, buf);
          break;
        case UNARY_MINUS:
          buffer_append(buf, "-");
          node_to_string_recurse(l->left, buf);
          break;
        case UNARY_NOT:
          buffer_append(buf, "~");
          node_to_string_recurse(l->left, buf);
          break;
        case BINARY_PLUS:
          node_to_string_recurse(l->left, buf);
          buffer_append(buf, " + ");
          node_to_string_recurse(l->right, buf);
          break;
        case BINARY_MINUS:
          node_to_string_recurse(l->left, buf);
          buffer_append(buf, " - ");
          node_to_string_recurse(l->right, buf);
          break;
        case BINARY_OR:
          node_to_string_recurse(l->left, buf);
          buffer_append(buf, " | ");
          node_to_string_recurse(l->right, buf);
          break;
        case BINARY_AND:
          node_to_string_recurse(l->left, buf);
          buffer_append(buf, " & ");
          node_to_string_recurse(l->right, buf);
          break;
        case BINARY_DIV:
          node_to_string_recurse(l->left, buf);
          buffer_append(buf, " / ");
          node_to_string_recurse(l->right, buf);
          break;
        case BINARY_MUL:
          node_to_string_recurse(l->left, buf);
          buffer_append(buf, " * ");
          node_to_string_recurse(l->right, buf);
          break;
        default:
          break;
      }

      if (l->is_ref)
        buffer_append(buf, ")");

      break;
    }
    case NODE_ID: {
      ID *l = (ID *)node;
      if (l->is_ref)
        buffer_append(buf, "(");

      buffer_append(buf, "%s", l->name);

      if (l->is_ref)
        buffer_append(buf, ")");
      break;
    }
    case NODE_LIST: {
      dynarray_cell *dc = NULL;
      LIST *l = (LIST *)node;

      if (l->list)
        foreach(dc, l->list) {
          node_to_string_recurse((parse_node *)dfirst(dc), buf);

          if (dc__state.i < (l->list->length - 1))
            buffer_append(buf, ", ");
        }

      break;
    }
    default:
      break;
  }
}

char *node_to_string(parse_node *node) {
  buffer *buf = buffer_init();
  node_to_string_recurse(node, buf);
  return buf->data;
}

int parse_string(struct libasm80_as_desc_t *desc, dynarray **statements, jmp_buf *parse_env) {
  struct yy_buffer_state *buffer;
  yyscan_t scanner;
  int result;

  g_includeopts = desc->includeopts;

  yylex_init(&scanner);
  buffer = yy_scan_string(desc->source, scanner);
  result = yyparse(scanner, statements, desc, parse_env);
  yy_delete_buffer(buffer, scanner);
  yylex_destroy(scanner);

  return result;
}

int parse_integer(struct libasm80_as_desc_t *desc, char *text, int len, int base, char suffix, jmp_buf *parse_env) {
  int result;
  char *tmp, *endptr;

  tmp = text;

  if (text[0] == '$')
    tmp = text + 1;
  else if ((text[0] == '0') && (text[1] == 'x'))
    tmp = text + 2;
  else if (text[len-1] == tolower(suffix) || text[len-1] == toupper(suffix)) {
    tmp = strdup(text);
    tmp[len-1] = 0;
  }

  result = strtol(tmp, &endptr, base);
  if (endptr == tmp) {
    if (desc->error_cb) {
      char *msg = bsprintf("error parse decimal integer: %s", text);
      int err_cb_ret = desc->error_cb(msg, 0);
      free(msg);
      if (err_cb_ret != 0)
        longjmp(*parse_env, 1);
    } else {
      result = 0;
    }
  }

  return result;
}

int parse_binary(struct libasm80_as_desc_t *desc, char *text, int len, jmp_buf *parse_env) {
  char *tmp;

  if (text[0] == '%') {
    tmp = text + 1;
  } else if ((text[0] == '0') && (text[1] == 'b')) {
    tmp = text + 2;
  } else if (tolower(text[len-1]) == 'b') {
    tmp = strdup(text);
    tmp[len - 1] = '\0';
  }

  int result;
  char *endptr;

  result = strtol(tmp, &endptr, 2);
  if (endptr == tmp) {
    if (desc->error_cb) {
      char *msg = bsprintf("error parse binary integer: %s", text);
      int err_cb_ret = desc->error_cb(msg, 0);
      free(msg);
      if (err_cb_ret != 0)
        longjmp(*parse_env, 1);
    } else {
      result = 0;
    }
  }

  return result;
}

void parse_print(dynarray *statements) {
  dynarray_cell *dc = NULL;

  foreach(dc, statements) {
    parse_node *node = (parse_node *)dfirst(dc);
    printf("%d: ", node->line);
    print_node(node);
    printf("\n");
  }
}

int parse_include(struct libasm80_as_desc_t *desc, dynarray **statements, char *filename, int line, jmp_buf *parse_env) {
  int ret = 0;
  size_t sz;
  FILE *fp = NULL;
  char *path;
  char *source = NULL;
  struct libasm80_as_desc_t desc_subtree;

  path = fs_abs_path(filename, g_includeopts);
  if (path == NULL) {
    if (desc->error_cb) {
      char *msg = bsprintf("file not found: %s", filename);
      ret = desc->error_cb(msg, line);
      free(msg);
    }

    goto out;
  }

  sz = fs_file_size(path);
  fp = fopen(path, "r");
  if (!fp) {
    if (desc->error_cb) {
      char *msg = bsprintf("%s: %s", path, strerror(errno));
      ret = desc->error_cb(msg, line);
      free(msg);
    }
    goto out;
  }

  source = (char *)malloc(sz);

  sz = fread(source, sz, 1, fp);
  if (sz != 1) {
    if (desc->error_cb) {
      char *msg = bsprintf("%s: %s", path, strerror(errno));
      ret = desc->error_cb(msg, line);
      free(msg);
    }
    goto out;
  }

  if (ret == 0) {
    memcpy(&desc_subtree, desc, sizeof(struct libasm80_as_desc_t));
    desc_subtree.source = source;
    ret = parse_string(&desc_subtree, statements, parse_env);
  }

out:
  if (source)
    free(source);
  if (fp)
    fclose(fp);

  return ret;
}

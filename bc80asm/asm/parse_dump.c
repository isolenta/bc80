#include "asm/bc80asm.h"
#include "asm/parse.h"
#include "bits/buffer.h"
#include "bits/dynarray.h"

const char *get_parse_node_name(parse_node *node)
{
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
    case NODE_REPT: {
      return "rept";
      break;
    }
    case NODE_ENDR: {
      return "endr";
      break;
    }
    case NODE_PROFILE: {
      return "profile";
      break;
    }
    case NODE_ENDPROFILE: {
      return "endprofile";
      break;
    }
    default:
      break;
  }
  return "unknown";
}

const char *get_literal_kind(LITERAL *l)
{
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

void print_node(parse_node *node)
{
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
        (node->is_ref ? "ref" : ""),
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
        case UNARY_INV:
          printf("uinv ");
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
        case BINARY_MOD:
          printf("mod ");
          break;
        case BINARY_SHL:
          printf("shl ");
          break;
        case BINARY_SHR:
          printf("shr ");
          break;
        case COND_EQ:
          printf("== ");
          break;
        case COND_NE:
          printf("!= ");
          break;
        case COND_LT:
          printf("< ");
          break;
        case COND_LE:
          printf("<= ");
          break;
        case COND_GT:
          printf("> ");
          break;
        case COND_GE:
          printf(">= ");
          break;
        default:
          break;
      }

      if (node->is_ref)
        printf("ref ");

      print_node(l->left);
      printf(" ");
      if (!(l->kind == SIMPLE || l->kind == UNARY_NOT || l->kind == UNARY_INV || l->kind == UNARY_PLUS || l->kind == UNARY_MINUS))
        print_node(l->right);

      printf(") ");

      break;
    }
    case NODE_ID: {
      ID *l = (ID *)node;
      printf("(ID %s %s) ", (node->is_ref ? "ref" : ""), l->name);
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
    case NODE_REPT: {
      REPT *r = (REPT *)node;
      printf("(REPT ");
      print_node((parse_node *)r->count_expr);
      if (r->var) {
        printf(" ");
        print_node((parse_node *)r->var);
      }
      printf(") ");
      break;
    }
    case NODE_ENDR:
      printf("(ENDR) ");
      break;
    case NODE_PROFILE: {
      PROFILE *p = (PROFILE *)node;

      printf("(PROFILE ");
      print_node((parse_node *)p->name);
      printf(") ");

      break;
    }
    case NODE_ENDPROFILE:
      printf("(ENDPROFILE) ");
      break;
    default:
      break;
  }
}

static void node_to_string_recurse(parse_node *node, buffer *buf)
{
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
      buffer_append(buf, "LITERAL ");

      if (node->is_ref)
        buffer_append(buf, "(");

      if (l->kind == INT)
        buffer_append(buf, "%d", l->ival);
      else if (l->kind == DOLLAR)
        buffer_append(buf, "$");
      else if (l->kind == STR)
        buffer_append(buf, "'%s'", l->strval);

      if (node->is_ref)
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

      if (node->is_ref)
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
          buffer_append(buf, "!");
          node_to_string_recurse(l->left, buf);
          break;
        case UNARY_INV:
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
        case BINARY_MOD:
          node_to_string_recurse(l->left, buf);
          buffer_append(buf, " % ");
          node_to_string_recurse(l->right, buf);
          break;
        case BINARY_SHL:
          node_to_string_recurse(l->left, buf);
          buffer_append(buf, " << ");
          node_to_string_recurse(l->right, buf);
          break;
        case BINARY_SHR:
          node_to_string_recurse(l->left, buf);
          buffer_append(buf, " >> ");
          node_to_string_recurse(l->right, buf);
          break;
        case COND_EQ:
          node_to_string_recurse(l->left, buf);
          buffer_append(buf, " == ");
          node_to_string_recurse(l->right, buf);
          break;
        case COND_NE:
          node_to_string_recurse(l->left, buf);
          buffer_append(buf, " != ");
          node_to_string_recurse(l->right, buf);
          break;
        case COND_LT:
          node_to_string_recurse(l->left, buf);
          buffer_append(buf, " < ");
          node_to_string_recurse(l->right, buf);
          break;
        case COND_LE:
          node_to_string_recurse(l->left, buf);
          buffer_append(buf, " <= ");
          node_to_string_recurse(l->right, buf);
          break;
        case COND_GT:
          node_to_string_recurse(l->left, buf);
          buffer_append(buf, " > ");
          node_to_string_recurse(l->right, buf);
          break;
        case COND_GE:
          node_to_string_recurse(l->left, buf);
          buffer_append(buf, " >= ");
          node_to_string_recurse(l->right, buf);
          break;
        default:
          break;
      }

      if (node->is_ref)
        buffer_append(buf, ")");

      break;
    }
    case NODE_ID: {
      ID *l = (ID *)node;
      buffer_append(buf, "ID ");
      if (node->is_ref)
        buffer_append(buf, "(");

      buffer_append(buf, "%s", l->name);

      if (node->is_ref)
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
    case NODE_REPT: {
      REPT *r = (REPT *)node;
      buffer_append(buf, "REPT ");
      node_to_string_recurse((parse_node *)r->count_expr, buf);
      if (r->var)
        buffer_append(buf, "%s ", r->var->name);
      break;
    }
    case NODE_ENDR:
      buffer_append(buf, "ENDR ");
      break;
    case NODE_PROFILE: {
      PROFILE *p = (PROFILE *)node;

      buffer_append(buf, "PROFILE ");
      node_to_string_recurse((parse_node *)p->name, buf);

      break;
    }
    case NODE_ENDPROFILE:
      buffer_append(buf, "ENDPROFILE ");
      break;
    default:
      break;
  }
}

char *node_to_string(parse_node *node)
{
  buffer *buf = buffer_init();
  node_to_string_recurse(node, buf);
  return buf->data;
}

void dump_parse_list(dynarray *statements)
{
  dynarray_cell *dc = NULL;

  foreach(dc, statements) {
    parse_node *node = (parse_node *)dfirst(dc);
    printf("%s:%d: ", node->fn, node->line);
    print_node(node);
    printf("\n");
  }
}

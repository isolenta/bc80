%{
    #include "asm/libasm.h"
    #include "asm/parse.h"
    #include "common/buffer.h"
    #include "common/common.h"
    #include "common/dynarray.h"
    #include "common/error.h"
    #include "common/mmgr.h"

    #define YY_DECL int yylex (yyscan_t yyscanner, struct libasm_as_desc_t *desc)
    #include "parser.tab.h"
    #include "lexer.yy.h"

    extern int yylex (yyscan_t yyscanner, struct libasm_as_desc_t *desc);

    void yyerror(yyscan_t scanner, dynarray **statements, struct libasm_as_desc_t *desc, const char *msg) {
      (void)scanner;
      (void)statements;
      generic_report_error(ERROR_OUT_LOC | ERROR_OUT_LINE,
        desc->filename, yylloc.first_line + 1, 0,
        (char *) msg);
    }
%}

%lex-param {void* scanner}{struct libasm_as_desc_t *desc}
%parse-param {void* scanner}{dynarray **statements}{struct libasm_as_desc_t *desc}

%union {
  char *str;
  int ival;
  parse_node *node;
}

%locations
%define parse.error custom

%token <str> T_ID T_STR
%token <ival> T_INT
%token T_DOLLAR T_LPAR T_RPAR T_MINUS T_PLUS T_MUL T_DIV T_COMMA T_COLON T_ORG T_EQU T_END T_DB
%token T_DM T_DW T_DS T_INCBIN T_INCLUDE T_NOT T_AND T_OR T_NL T_SECTION T_PERCENT T_SHL T_SHR
%token T_REPT T_ENDR T_PROFILE T_ENDPROFILE

%type <node> id str integer dollar simple_expr unary_expr expr exprlist

%start program

%%

str
      : T_STR {
        LITERAL *l = make_node(LITERAL, desc->filename, @1.first_line);
        l->kind = STR;
        l->strval = $1;
        $$ = (parse_node *)l;
      }
      ;

integer
      : T_INT {
        LITERAL *l = make_node(LITERAL, desc->filename, @1.first_line);
        l->kind = INT;
        l->ival = $1;
        $$ = (parse_node *)l;
      }
      ;

dollar
      : T_DOLLAR {
        LITERAL *l = make_node(LITERAL, desc->filename, @1.first_line);
        l->kind = DOLLAR;
        $$ = (parse_node *)l;
      }
      ;

id
      : T_ID {
        ID *l = make_node(ID, desc->filename, @1.first_line);
        l->name = $1;
        l->is_ref = false;
        $$ = (parse_node *)l;
      }
      ;

simple_expr
      : id {
        EXPR *l = make_node(EXPR, desc->filename, @1.first_line);
        l->kind = SIMPLE;
        l->is_ref = false;
        l->left = $1;
        l->right = NULL;
        $$ = (parse_node *)l;
      }
      | str { $$ = $1; }
      | integer { $$ = $1; }
      | dollar { $$ = $1; }
      | T_LPAR expr T_RPAR {
        $$ = $2;
        ((EXPR *)$$)->is_ref = true;
      }
      ;

unary_expr
      : simple_expr { $$ = $1; }
      | T_PLUS simple_expr {
        EXPR *l = make_node(EXPR, desc->filename, @2.first_line);
        l->kind = UNARY_PLUS;
        l->is_ref = false;
        l->left = $2;
        l->right = NULL;
        $$ = (parse_node *)l;
      }
      | T_MINUS simple_expr {
        EXPR *l = make_node(EXPR, desc->filename, @2.first_line);
        l->kind = UNARY_MINUS;
        l->is_ref = false;
        l->left = $2;
        l->right = NULL;
        $$ = (parse_node *)l;
      }
      | T_NOT simple_expr {
        EXPR *l = make_node(EXPR, desc->filename, @2.first_line);
        l->kind = UNARY_NOT;
        l->is_ref = false;
        l->left = $2;
        l->right = NULL;
        $$ = (parse_node *)l;
      }
      ;

expr
      : unary_expr { $$ = $1; }
      | unary_expr T_PLUS unary_expr {
        EXPR *l = make_node(EXPR, desc->filename, @1.first_line);
        l->kind = BINARY_PLUS;
        l->is_ref = false;
        l->left = $1;
        l->right = $3;
        $$ = (parse_node *)l;
      }
      | unary_expr T_MINUS unary_expr {
        EXPR *l = make_node(EXPR, desc->filename, @1.first_line);
        l->kind = BINARY_MINUS;
        l->is_ref = false;
        l->left = $1;
        l->right = $3;
        $$ = (parse_node *)l;
      }
      | unary_expr T_MUL unary_expr {
        EXPR *l = make_node(EXPR, desc->filename, @1.first_line);
        l->kind = BINARY_MUL;
        l->is_ref = false;
        l->left = $1;
        l->right = $3;
        $$ = (parse_node *)l;
      }
      | unary_expr T_DIV unary_expr {
        EXPR *l = make_node(EXPR, desc->filename, @1.first_line);
        l->kind = BINARY_DIV;
        l->is_ref = false;
        l->left = $1;
        l->right = $3;
        $$ = (parse_node *)l;
      }
      | unary_expr T_AND unary_expr {
        EXPR *l = make_node(EXPR, desc->filename, @1.first_line);
        l->kind = BINARY_AND;
        l->is_ref = false;
        l->left = $1;
        l->right = $3;
        $$ = (parse_node *)l;
      }
      | unary_expr T_OR unary_expr {
        EXPR *l = make_node(EXPR, desc->filename, @1.first_line);
        l->kind = BINARY_OR;
        l->is_ref = false;
        l->left = $1;
        l->right = $3;
        $$ = (parse_node *)l;
      }
      | unary_expr T_PERCENT unary_expr {
        EXPR *l = make_node(EXPR, desc->filename, @1.first_line);
        l->kind = BINARY_MOD;
        l->is_ref = false;
        l->left = $1;
        l->right = $3;
        $$ = (parse_node *)l;
      }
      | unary_expr T_SHL unary_expr {
        EXPR *l = make_node(EXPR, desc->filename, @1.first_line);
        l->kind = BINARY_SHL;
        l->is_ref = false;
        l->left = $1;
        l->right = $3;
        $$ = (parse_node *)l;
      }
      | unary_expr T_SHR unary_expr {
        EXPR *l = make_node(EXPR, desc->filename, @1.first_line);
        l->kind = BINARY_SHR;
        l->is_ref = false;
        l->left = $1;
        l->right = $3;
        $$ = (parse_node *)l;
      }

exprlist
      : expr {
        LIST *l = make_node(LIST, desc->filename, @1.first_line);
        l->list = NULL;
        l->list = dynarray_append_ptr(l->list, $1);
        $$ = (parse_node *)l;
      }
      | exprlist T_COMMA expr {
        LIST *l = (LIST *)$$;
        l->list = dynarray_append_ptr(l->list, $3);
      }
      ;

label
      : id T_COLON {
          LABEL *l = make_node(LABEL, desc->filename, @1.first_line);
          l->name = (ID *)$1;
          *statements = dynarray_append_ptr(*statements, l);
        }

stmt
      : label
      | T_ORG expr {
        ORG *l = make_node(ORG, desc->filename, @2.first_line);
        l->value = (parse_node *)$2;
        *statements = dynarray_append_ptr(*statements, l);
      }
      | T_END {
        END *l = make_node(END, desc->filename, @1.first_line);
        *statements = dynarray_append_ptr(*statements, l);
      }
      | T_REPT integer {
        REPT *l = make_node(REPT, desc->filename, @1.first_line);
        l->count = (LITERAL *)$2;
        *statements = dynarray_append_ptr(*statements, l);
      }
      | T_ENDR {
        ENDR *l = make_node(ENDR, desc->filename, @1.first_line);
        *statements = dynarray_append_ptr(*statements, l);
      }
      | T_PROFILE str {
        PROFILE *l = make_node(PROFILE, desc->filename, @1.first_line);
        l->name = (LITERAL *)$2;
        *statements = dynarray_append_ptr(*statements, l);
      }
      | T_ENDPROFILE {
        ENDPROFILE *l = make_node(ENDPROFILE, desc->filename, @1.first_line);
        *statements = dynarray_append_ptr(*statements, l);
      }
      | id T_COLON T_EQU expr {
        EQU *l = make_node(EQU, desc->filename, @1.first_line);
        l->name = (ID *)$1;
        l->value = (EXPR *)$4;
        *statements = dynarray_append_ptr(*statements, l);
      }
      | id T_EQU expr {
        EQU *l = make_node(EQU, desc->filename, @1.first_line);
        l->name = (ID *)$1;
        l->value = (EXPR *)$3;
        *statements = dynarray_append_ptr(*statements, l);
      }
      | T_INCBIN str {
        INCBIN *l = make_node(INCBIN, desc->filename, @1.first_line);
        l->filename = (LITERAL *)$2;
        *statements = dynarray_append_ptr(*statements, l);
      }
      | T_INCLUDE str {
        LITERAL *filename = (LITERAL *)$2;
        parse_include(desc, statements, filename->strval, @1.first_line);
      }
      | T_DB exprlist {
        DEF *l = make_node(DEF, desc->filename, @1.first_line);
        l->kind = DEFKIND_DB;
        l->values = (LIST *)$2;
        *statements = dynarray_append_ptr(*statements, l);
      }
      | T_DM exprlist {
        DEF *l = make_node(DEF, desc->filename, @1.first_line);
        l->kind = DEFKIND_DM;
        l->values = (LIST *)$2;
        *statements = dynarray_append_ptr(*statements, l);
      }
      | T_DW exprlist {
        DEF *l = make_node(DEF, desc->filename, @1.first_line);
        l->kind = DEFKIND_DW;
        l->values = (LIST *)$2;
        *statements = dynarray_append_ptr(*statements, l);
      }
      | T_DS exprlist {
        DEF *l = make_node(DEF, desc->filename, @1.first_line);
        l->kind = DEFKIND_DS;
        l->values = (LIST *)$2;
        *statements = dynarray_append_ptr(*statements, l);
      }
      | T_SECTION exprlist {
        SECTION *l = make_node(SECTION, desc->filename, @1.first_line);
        l->args = (LIST *)$2;
        *statements = dynarray_append_ptr(*statements, l);
      }
      | id exprlist {
        INSTR *l = make_node(INSTR, desc->filename, @1.first_line);
        l->name = (ID *)$1;
        l->args = (LIST *)$2;
        *statements = dynarray_append_ptr(*statements, l);
      }
      | id {
        INSTR *l = make_node(INSTR, desc->filename, @1.first_line);
        l->name = (ID *)$1;
        l->args = NULL;
        *statements = dynarray_append_ptr(*statements, l);
      }
      ;

nl
      : T_NL
      ;

cstmt
      : stmt nl
      | label stmt nl
      | nl {}
      ;

program
      : cstmt
      | program cstmt
      ;

%%

static const char *yysymbol_name_human(yysymbol_kind_t yysymbol) {
  switch (yysymbol) {
    case YYSYMBOL_T_ID:
      return "identifier";
    case YYSYMBOL_T_INT:
      return "integer";
    case YYSYMBOL_T_STR:
      return "string";
    case YYSYMBOL_T_DOLLAR:
      return "$";
    case YYSYMBOL_T_INCBIN:
      return "INCBIN";
    case YYSYMBOL_T_INCLUDE:
      return "INCLUDE";
    case YYSYMBOL_T_NL:
      return "end of line";
    case YYSYMBOL_T_SECTION:
      return "SECTION";
    case YYSYMBOL_T_REPT:
      return "REPT";
    case YYSYMBOL_T_ENDR:
      return "ENDR";
    case YYSYMBOL_T_PROFILE:
      return "PROFILE";
    case YYSYMBOL_T_ENDPROFILE:
      return "ENDPROFILE";
    default:
      return yysymbol_name(yysymbol);
  }
}

static int yyreport_syntax_error(const yypcontext_t *yyctx, void* scanner, dynarray **statements, struct libasm_as_desc_t *desc)
{
  buffer *errmsg = buffer_init();
  int res = 0, num_expected;
  enum { TOKENMAX = 5 };
  yysymbol_kind_t expected[TOKENMAX];

  num_expected = yypcontext_expected_tokens(yyctx, expected, TOKENMAX);
  if (num_expected < 0) {
    res = num_expected;    // forward errors to yyparse
  } else {
    for (int i = 0; i < num_expected; ++i)
      buffer_append(errmsg, "%s %s",
               i == 0 ? "expected" : " or", yysymbol_name_human(expected[i]));
  }

  yysymbol_kind_t lookahead = yypcontext_token(yyctx);
  if (lookahead != YYSYMBOL_YYEMPTY) {
    if (lookahead == YYSYMBOL_YYEOF) {
      buffer_append(errmsg, " until the end of file");
    } else {
      YYLTYPE *loc = yypcontext_location(yyctx);

      // TODO: follow column position in lexer
      char *err_token = get_token_at(desc->source, loc->first_line, loc->first_column, loc->last_column - loc->first_column + 1);
      if (num_expected > 0)
        /* buffer_append(errmsg, " before '%s'", err_token); */ ;
      else
        buffer_append(errmsg, "unexpected %s", yysymbol_name_human(lookahead));
    }
  }

  char *errstr = buffer_dup(errmsg);
  buffer_free(errmsg);

  generic_report_error(ERROR_OUT_LOC | ERROR_OUT_LINE,
    desc->filename, yylloc.first_line + 1, 0,
    errstr);
  return res;
}

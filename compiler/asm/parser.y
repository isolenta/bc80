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
        LITERAL *l = make_node(LITERAL, desc->filename, @1.first_line, @1.first_column);
        l->kind = STR;
        l->strval = $1;
        $$ = (parse_node *)l;
      }
      ;

integer
      : T_INT {
        LITERAL *l = make_node(LITERAL, desc->filename, @1.first_line, @1.first_column);
        l->kind = INT;
        l->ival = $1;
        $$ = (parse_node *)l;
      }
      ;

dollar
      : T_DOLLAR {
        LITERAL *l = make_node(LITERAL, desc->filename, @1.first_line, @1.first_column);
        l->kind = DOLLAR;
        $$ = (parse_node *)l;
      }
      ;

id
      : T_ID {
        ID *l = make_node(ID, desc->filename, @1.first_line, @1.first_column);
        l->name = $1;
        l->is_ref = false;
        $$ = (parse_node *)l;
      }
      ;

simple_expr
      : id {
        EXPR *l = make_node(EXPR, desc->filename, @1.first_line, @1.first_column);
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
      | T_PLUS unary_expr {
        EXPR *l = make_node(EXPR, desc->filename, @2.first_line, @1.first_column);
        l->kind = UNARY_PLUS;
        l->is_ref = false;
        l->left = $2;
        l->right = NULL;
        $$ = (parse_node *)l;
      }
      | T_MINUS unary_expr {
        EXPR *l = make_node(EXPR, desc->filename, @2.first_line, @1.first_column);
        l->kind = UNARY_MINUS;
        l->is_ref = false;
        l->left = $2;
        l->right = NULL;
        $$ = (parse_node *)l;
      }
      | T_NOT unary_expr {
        EXPR *l = make_node(EXPR, desc->filename, @2.first_line, @1.first_column);
        l->kind = UNARY_NOT;
        l->is_ref = false;
        l->left = $2;
        l->right = NULL;
        $$ = (parse_node *)l;
      }
      ;

expr
      : unary_expr { $$ = $1; }
      | expr T_PLUS expr {
        EXPR *l = make_node(EXPR, desc->filename, @1.first_line, @1.first_column);
        l->kind = BINARY_PLUS;
        l->is_ref = false;
        l->left = $1;
        l->right = $3;
        $$ = (parse_node *)l;
      }
      | expr T_MINUS expr {
        EXPR *l = make_node(EXPR, desc->filename, @1.first_line, @1.first_column);
        l->kind = BINARY_MINUS;
        l->is_ref = false;
        l->left = $1;
        l->right = $3;
        $$ = (parse_node *)l;
      }
      | expr T_MUL expr {
        EXPR *l = make_node(EXPR, desc->filename, @1.first_line, @1.first_column);
        l->kind = BINARY_MUL;
        l->is_ref = false;
        l->left = $1;
        l->right = $3;
        $$ = (parse_node *)l;
      }
      | expr T_DIV expr {
        EXPR *l = make_node(EXPR, desc->filename, @1.first_line, @1.first_column);
        l->kind = BINARY_DIV;
        l->is_ref = false;
        l->left = $1;
        l->right = $3;
        $$ = (parse_node *)l;
      }
      | expr T_AND expr {
        EXPR *l = make_node(EXPR, desc->filename, @1.first_line, @1.first_column);
        l->kind = BINARY_AND;
        l->is_ref = false;
        l->left = $1;
        l->right = $3;
        $$ = (parse_node *)l;
      }
      | expr T_OR expr {
        EXPR *l = make_node(EXPR, desc->filename, @1.first_line, @1.first_column);
        l->kind = BINARY_OR;
        l->is_ref = false;
        l->left = $1;
        l->right = $3;
        $$ = (parse_node *)l;
      }
      | expr T_PERCENT expr {
        EXPR *l = make_node(EXPR, desc->filename, @1.first_line, @1.first_column);
        l->kind = BINARY_MOD;
        l->is_ref = false;
        l->left = $1;
        l->right = $3;
        $$ = (parse_node *)l;
      }
      | expr T_SHL expr {
        EXPR *l = make_node(EXPR, desc->filename, @1.first_line, @1.first_column);
        l->kind = BINARY_SHL;
        l->is_ref = false;
        l->left = $1;
        l->right = $3;
        $$ = (parse_node *)l;
      }
      | expr T_SHR expr {
        EXPR *l = make_node(EXPR, desc->filename, @1.first_line, @1.first_column);
        l->kind = BINARY_SHR;
        l->is_ref = false;
        l->left = $1;
        l->right = $3;
        $$ = (parse_node *)l;
      }

exprlist
      : expr {
        LIST *l = make_node(LIST, desc->filename, @1.first_line, @1.first_column);
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
          LABEL *l = make_node(LABEL, desc->filename, @1.first_line, @1.first_column);
          l->name = (ID *)$1;
          *statements = dynarray_append_ptr(*statements, l);
        }

stmt
      : label
      | T_ORG expr {
        ORG *l = make_node(ORG, desc->filename, @2.first_line, @1.first_column);
        l->value = (parse_node *)$2;
        *statements = dynarray_append_ptr(*statements, l);
      }
      | T_END {
        END *l = make_node(END, desc->filename, @1.first_line, @1.first_column);
        *statements = dynarray_append_ptr(*statements, l);
      }
      | T_REPT integer {
        REPT *l = make_node(REPT, desc->filename, @1.first_line, @1.first_column);
        l->count = (LITERAL *)$2;
        l->var = NULL;
        *statements = dynarray_append_ptr(*statements, l);
      }
      | T_REPT integer T_COMMA id {
        REPT *l = make_node(REPT, desc->filename, @1.first_line, @1.first_column);
        l->count = (LITERAL *)$2;
        l->var = (ID *)$4;
        *statements = dynarray_append_ptr(*statements, l);
      }
      | T_ENDR {
        ENDR *l = make_node(ENDR, desc->filename, @1.first_line, @1.first_column);
        *statements = dynarray_append_ptr(*statements, l);
      }
      | T_PROFILE str {
        PROFILE *l = make_node(PROFILE, desc->filename, @1.first_line, @1.first_column);
        l->name = (LITERAL *)$2;
        *statements = dynarray_append_ptr(*statements, l);
      }
      | T_ENDPROFILE {
        ENDPROFILE *l = make_node(ENDPROFILE, desc->filename, @1.first_line, @1.first_column);
        *statements = dynarray_append_ptr(*statements, l);
      }
      | id T_COLON T_EQU expr {
        EQU *l = make_node(EQU, desc->filename, @1.first_line, @1.first_column);
        l->name = (ID *)$1;
        l->value = (EXPR *)$4;
        *statements = dynarray_append_ptr(*statements, l);
      }
      | id T_EQU expr {
        EQU *l = make_node(EQU, desc->filename, @1.first_line, @1.first_column);
        l->name = (ID *)$1;
        l->value = (EXPR *)$3;
        *statements = dynarray_append_ptr(*statements, l);
      }
      | T_INCBIN str {
        INCBIN *l = make_node(INCBIN, desc->filename, @1.first_line, @1.first_column);
        l->filename = (LITERAL *)$2;
        *statements = dynarray_append_ptr(*statements, l);
      }
      | T_INCLUDE str {
        LITERAL *filename = (LITERAL *)$2;
        parse_include(desc, statements, filename->strval);
      }
      | T_DB exprlist {
        DEF *l = make_node(DEF, desc->filename, @1.first_line, @1.first_column);
        l->kind = DEFKIND_DB;
        l->values = (LIST *)$2;
        *statements = dynarray_append_ptr(*statements, l);
      }
      | T_DM exprlist {
        DEF *l = make_node(DEF, desc->filename, @1.first_line, @1.first_column);
        l->kind = DEFKIND_DM;
        l->values = (LIST *)$2;
        *statements = dynarray_append_ptr(*statements, l);
      }
      | T_DW exprlist {
        DEF *l = make_node(DEF, desc->filename, @1.first_line, @1.first_column);
        l->kind = DEFKIND_DW;
        l->values = (LIST *)$2;
        *statements = dynarray_append_ptr(*statements, l);
      }
      | T_DS exprlist {
        DEF *l = make_node(DEF, desc->filename, @1.first_line, @1.first_column);
        l->kind = DEFKIND_DS;
        l->values = (LIST *)$2;
        *statements = dynarray_append_ptr(*statements, l);
      }
      | T_SECTION exprlist {
        SECTION *l = make_node(SECTION, desc->filename, @1.first_line, @1.first_column);
        l->args = (LIST *)$2;
        *statements = dynarray_append_ptr(*statements, l);
      }
      | id exprlist {
        INSTR *l = make_node(INSTR, desc->filename, @1.first_line, @1.first_column);
        l->name = (ID *)$1;
        l->args = (LIST *)$2;
        *statements = dynarray_append_ptr(*statements, l);
      }
      | id {
        INSTR *l = make_node(INSTR, desc->filename, @1.first_line, @1.first_column);
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
    buffer_append(errmsg, "syntax error");
  }

  yysymbol_kind_t lookahead = yypcontext_token(yyctx);
  if (lookahead != YYSYMBOL_YYEMPTY) {
    if (lookahead == YYSYMBOL_YYEOF) {
      buffer_append(errmsg, " at the end of file");
    } else {
      YYLTYPE *loc = yypcontext_location(yyctx);

      char *err_token = get_token_at(desc->source, loc->first_line, loc->first_column, loc->last_column - loc->first_column + 1);
      if (num_expected > 0)
        buffer_append(errmsg, " near '%s'", err_token);
    }
  }

  char *errstr = buffer_dup(errmsg);
  buffer_free(errmsg);

  generic_report_error(ERROR_OUT_LOC | ERROR_OUT_LINE | ERROR_OUT_POS,
    desc->filename, yylloc.first_line, yylloc.first_column,
    errstr);
  return res;
}

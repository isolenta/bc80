%{
    #include <stdio.h>
    #include <stdio.h>
    #include <setjmp.h>

    #include "dynarray.h"
    #include "parse.h"
    #include "libasm80.h"

    #define YY_DECL int yylex (yyscan_t yyscanner, struct libasm80_as_desc_t *desc, jmp_buf *parse_env)
    #include "parser.tab.h"
    #include "lexer.yy.h"

    extern int yylex (yyscan_t yyscanner, struct libasm80_as_desc_t *desc, jmp_buf *parse_env);

    void yyerror(yyscan_t scanner, dynarray **statements, struct libasm80_as_desc_t *desc, jmp_buf *parse_env, const char *msg) {
      (void)scanner;
      (void)statements;

      if (desc->error_cb) {
        int err_cb_ret = desc->error_cb(msg, yylloc.first_line + 1);
        if (err_cb_ret != 0)
          longjmp(*parse_env, 1);
      }
    }
%}

%lex-param {void* scanner}{struct libasm80_as_desc_t *desc}{jmp_buf *parse_env}
%parse-param {void* scanner}{dynarray **statements}{struct libasm80_as_desc_t *desc}{jmp_buf *parse_env}

%union {
  char *str;
  int ival;
  void *node;
}

%locations
%define parse.error verbose

%token <str> T_ID T_STR
%token <ival> T_INT T_DOLLAR
%token T_LPAR T_RPAR T_MINUS T_PLUS T_MUL T_DIV T_COMMA T_COLON T_ORG T_EQU T_END T_DB T_DM T_DW T_DS
%token T_INCBIN T_INCLUDE T_NOT T_AND T_OR T_NL T_SECTION T_PERCENT T_SHL T_SHR

%type <node> program cstmt stmt label id str integer dollar simple_expr unary_expr expr exprlist

%start program

%%

str
      : T_STR {
        LITERAL *l = make_node(LITERAL, @1.first_line);
        l->kind = STR;
        l->strval = strdup($1);
        $$ = (parse_node *)l;
      }
      ;

integer
      : T_INT {
        LITERAL *l = make_node(LITERAL, @1.first_line);
        l->kind = INT;
        l->ival = $1;
        $$ = (parse_node *)l;
      }
      ;

dollar
      : T_DOLLAR {
        LITERAL *l = make_node(LITERAL, @1.first_line);
        l->kind = DOLLAR;
        $$ = (parse_node *)l;
      }
      ;

id
      : T_ID {
        ID *l = make_node(ID, @1.first_line);
        l->name = strdup($1);
        l->is_ref = false;
        $$ = (parse_node *)l;
      }
      ;

simple_expr
      : id {
        EXPR *l = make_node(EXPR, @1.first_line);
        l->kind = SIMPLE;
        l->is_ref = false;
        l->left = $1;
        l->right = NULL;
        $$ = (parse_node *)l;
      }
      | T_LPAR id T_RPAR {
        EXPR *l = make_node(EXPR, @2.first_line);
        l->kind = SIMPLE;
        l->is_ref = true;
        l->left = $2;
        l->right = NULL;
        $$ = (parse_node *)l;
      }
      | str { $$ = $1; }
      | T_LPAR integer T_RPAR {
         $$ = $2;
         ((LITERAL *)$$)->is_ref = true;
      }
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
        EXPR *l = make_node(EXPR, @2.first_line);
        l->kind = UNARY_PLUS;
        l->is_ref = false;
        l->left = $2;
        l->right = NULL;
        $$ = (parse_node *)l;
      }
      | T_MINUS simple_expr {
        EXPR *l = make_node(EXPR, @2.first_line);
        l->kind = UNARY_MINUS;
        l->is_ref = false;
        l->left = $2;
        l->right = NULL;
        $$ = (parse_node *)l;
      }
      | T_NOT simple_expr {
        EXPR *l = make_node(EXPR, @2.first_line);
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
        EXPR *l = make_node(EXPR, @1.first_line);
        l->kind = BINARY_PLUS;
        l->is_ref = false;
        l->left = $1;
        l->right = $3;
        $$ = (parse_node *)l;
      }
      | unary_expr T_MINUS unary_expr {
        EXPR *l = make_node(EXPR, @1.first_line);
        l->kind = BINARY_MINUS;
        l->is_ref = false;
        l->left = $1;
        l->right = $3;
        $$ = (parse_node *)l;
      }
      | unary_expr T_MUL unary_expr {
        EXPR *l = make_node(EXPR, @1.first_line);
        l->kind = BINARY_MUL;
        l->is_ref = false;
        l->left = $1;
        l->right = $3;
        $$ = (parse_node *)l;
      }
      | unary_expr T_DIV unary_expr {
        EXPR *l = make_node(EXPR, @1.first_line);
        l->kind = BINARY_DIV;
        l->is_ref = false;
        l->left = $1;
        l->right = $3;
        $$ = (parse_node *)l;
      }
      | unary_expr T_AND unary_expr {
        EXPR *l = make_node(EXPR, @1.first_line);
        l->kind = BINARY_AND;
        l->is_ref = false;
        l->left = $1;
        l->right = $3;
        $$ = (parse_node *)l;
      }
      | unary_expr T_OR unary_expr {
        EXPR *l = make_node(EXPR, @1.first_line);
        l->kind = BINARY_OR;
        l->is_ref = false;
        l->left = $1;
        l->right = $3;
        $$ = (parse_node *)l;
      }
      | unary_expr T_PERCENT unary_expr {
        EXPR *l = make_node(EXPR, @1.first_line);
        l->kind = BINARY_MOD;
        l->is_ref = false;
        l->left = $1;
        l->right = $3;
        $$ = (parse_node *)l;
      }
      | unary_expr T_SHL unary_expr {
        EXPR *l = make_node(EXPR, @1.first_line);
        l->kind = BINARY_SHL;
        l->is_ref = false;
        l->left = $1;
        l->right = $3;
        $$ = (parse_node *)l;
      }
      | unary_expr T_SHR unary_expr {
        EXPR *l = make_node(EXPR, @1.first_line);
        l->kind = BINARY_SHR;
        l->is_ref = false;
        l->left = $1;
        l->right = $3;
        $$ = (parse_node *)l;
      }

exprlist
      : expr {
        LIST *l = make_node(LIST, @1.first_line);
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
          LABEL *l = make_node(LABEL, @1.first_line);
          l->name = (ID *)$1;
          *statements = dynarray_append_ptr(*statements, l);
        }

stmt
      : label
      | T_ORG expr {
        ORG *l = make_node(ORG, @2.first_line);
        l->value = (parse_node *)$2;
        *statements = dynarray_append_ptr(*statements, l);
      }
      | T_END {
        END *l = make_node(END, @1.first_line);
        *statements = dynarray_append_ptr(*statements, l);
      }
      | id T_COLON T_EQU expr {
        EQU *l = make_node(EQU, @1.first_line);
        l->name = (ID *)$1;
        l->value = (EXPR *)$4;
        *statements = dynarray_append_ptr(*statements, l);
      }
      | id T_EQU expr {
        EQU *l = make_node(EQU, @1.first_line);
        l->name = (ID *)$1;
        l->value = (EXPR *)$3;
        *statements = dynarray_append_ptr(*statements, l);
      }
      | T_INCBIN str {
        INCBIN *l = make_node(INCBIN, @1.first_line);
        l->filename = (LITERAL *)$2;
        *statements = dynarray_append_ptr(*statements, l);
      }
      | T_INCLUDE str {
        LITERAL *filename = (LITERAL *)$2;
        if (parse_include(desc, statements, filename->strval, @1.first_line, parse_env) != 0)
          longjmp(*parse_env, 1);
      }
      | T_DB exprlist {
        DEF *l = make_node(DEF, @1.first_line);
        l->kind = DEFKIND_DB;
        l->values = (LIST *)$2;
        *statements = dynarray_append_ptr(*statements, l);
      }
      | T_DM exprlist {
        DEF *l = make_node(DEF, @1.first_line);
        l->kind = DEFKIND_DM;
        l->values = (LIST *)$2;
        *statements = dynarray_append_ptr(*statements, l);
      }
      | T_DW exprlist {
        DEF *l = make_node(DEF, @1.first_line);
        l->kind = DEFKIND_DW;
        l->values = (LIST *)$2;
        *statements = dynarray_append_ptr(*statements, l);
      }
      | T_DS exprlist {
        DEF *l = make_node(DEF, @1.first_line);
        l->kind = DEFKIND_DS;
        l->values = (LIST *)$2;
        *statements = dynarray_append_ptr(*statements, l);
      }
      | T_SECTION exprlist {
        SECTION *l = make_node(SECTION, @1.first_line);
        l->args = (LIST *)$2;
        *statements = dynarray_append_ptr(*statements, l);
      }
      | id exprlist {
        INSTR *l = make_node(INSTR, @1.first_line);
        l->name = (ID *)$1;
        l->args = (LIST *)$2;
        *statements = dynarray_append_ptr(*statements, l);
      }
      | id {
        INSTR *l = make_node(INSTR, @1.first_line);
        l->name = (ID *)$1;
        l->args = NULL;
        *statements = dynarray_append_ptr(*statements, l);
      }
      ;

nl
      : T_NL
      | nl T_NL
      ;

cstmt
      : stmt nl
      | label stmt nl
      | nl
      ;

program
      : cstmt
      | program cstmt
      ;

%%

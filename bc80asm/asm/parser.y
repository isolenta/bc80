%{
    #include "asm/bc80asm.h"
    #include "asm/parse.h"
    #include "bits/buffer.h"
    #include "bits/common.h"
    #include "bits/dynarray.h"
    #include "bits/error.h"
    #include "bits/mmgr.h"

    #define YY_DECL int yylex (yyscan_t yyscanner, char *filename, char *source)
    #include "parser.tab.h"
    #include "lexer.yy.h"

    extern int yylex (yyscan_t yyscanner, char *filename, char *source);

    void yyerror(yyscan_t scanner, dynarray **statements, char *filename, char *source, const char *msg)
    {
      (void)scanner;
      (void)statements;
      generic_report_error(ERROR_OUT_LOC | ERROR_OUT_LINE,
        filename, yylloc.first_line + 1, 0,
        (char *) msg);
    }

    #define RULE_EXPRESSION(target, _kind, _left, _right, _line, _column)  \
      do {                                                                 \
        EXPR *l = make_node(EXPR, filename, _line, _column);               \
        l->kind = _kind;                                                   \
        l->hdr.is_ref = false;                                             \
        l->left = _left;                                                   \
        l->right = _right;                                                 \
        target = (parse_node *)l;                                          \
      } while (0)

%}

%lex-param {void* scanner}{char *filename}{char *source}
%parse-param {void* scanner}{dynarray **statements}{char *filename}{char *source}

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
%token T_DM T_DW T_DS T_INCBIN T_INCLUDE T_NOT T_INV T_AND T_OR T_NL T_SECTION T_PERCENT T_SHL T_SHR
%token T_REPT T_ENDR T_PROFILE T_ENDPROFILE T_IF T_ELSE T_ENDIF
%token T_EQ T_NE T_LT T_LE T_GT T_GE

%type <node> id str integer dollar simple_expr unary_expr expr exprlist keyvalue kvlist

%start program

%%

str
      : T_STR {
        LITERAL *l = make_node(LITERAL, filename, @1.first_line, @1.first_column);
        l->kind = STR;
        l->strval = $1;
        $$ = (parse_node *)l;
      }
      ;

integer
      : T_INT {
        LITERAL *l = make_node(LITERAL, filename, @1.first_line, @1.first_column);
        l->kind = INT;
        l->ival = $1;
        $$ = (parse_node *)l;
      }
      ;

dollar
      : T_DOLLAR {
        LITERAL *l = make_node(LITERAL, filename, @1.first_line, @1.first_column);
        l->kind = DOLLAR;
        $$ = (parse_node *)l;
      }
      ;

id
      : T_ID {
        ID *l = make_node(ID, filename, @1.first_line, @1.first_column);
        l->name = $1;
        l->hdr.is_ref = false;
        $$ = (parse_node *)l;
      }
      ;

simple_expr
      : id {
        EXPR *l = make_node(EXPR, filename, @1.first_line, @1.first_column);
        l->kind = SIMPLE;
        l->hdr.is_ref = false;
        l->left = $1;
        l->right = NULL;
        $$ = (parse_node *)l;
      }
      | str { $$ = $1; }
      | integer { $$ = $1; }
      | dollar { $$ = $1; }
      | T_LPAR expr T_RPAR {
        $$ = $2;
        ((EXPR *)$$)->hdr.is_ref = true;
      }
      ;

unary_expr
      : simple_expr { $$ = $1; }
      | T_PLUS unary_expr {
        RULE_EXPRESSION($$, UNARY_PLUS, $2, NULL, @1.first_line, @1.first_column);
      }
      | T_MINUS unary_expr {
        RULE_EXPRESSION($$, UNARY_MINUS, $2, NULL, @1.first_line, @1.first_column);
      }
      | T_NOT unary_expr {
        RULE_EXPRESSION($$, UNARY_NOT, $2, NULL, @1.first_line, @1.first_column);
      }
      | T_INV unary_expr {
        RULE_EXPRESSION($$, UNARY_INV, $2, NULL, @1.first_line, @1.first_column);
      }
      ;

expr
      : unary_expr { $$ = $1; }
      | expr T_PLUS expr {
        RULE_EXPRESSION($$, BINARY_PLUS, $1, $3, @1.first_line, @1.first_column);
      }
      | expr T_MINUS expr {
        RULE_EXPRESSION($$, BINARY_MINUS, $1, $3, @1.first_line, @1.first_column);
      }
      | expr T_MUL expr {
        RULE_EXPRESSION($$, BINARY_MUL, $1, $3, @1.first_line, @1.first_column);
      }
      | expr T_DIV expr {
        RULE_EXPRESSION($$, BINARY_DIV, $1, $3, @1.first_line, @1.first_column);
      }
      | expr T_AND expr {
        RULE_EXPRESSION($$, BINARY_AND, $1, $3, @1.first_line, @1.first_column);
      }
      | expr T_OR expr {
        RULE_EXPRESSION($$, BINARY_OR, $1, $3, @1.first_line, @1.first_column);
      }
      | expr T_PERCENT expr {
        RULE_EXPRESSION($$, BINARY_MOD, $1, $3, @1.first_line, @1.first_column);
      }
      | expr T_SHL expr {
        RULE_EXPRESSION($$, BINARY_SHL, $1, $3, @1.first_line, @1.first_column);
      }
      | expr T_SHR expr {
        RULE_EXPRESSION($$, BINARY_SHR, $1, $3, @1.first_line, @1.first_column);
      }
      | expr T_EQ expr {
        RULE_EXPRESSION($$, COND_EQ, $1, $3, @1.first_line, @1.first_column);
      }
      | expr T_NE expr {
        RULE_EXPRESSION($$, COND_NE, $1, $3, @1.first_line, @1.first_column);
      }
      | expr T_LT expr {
        RULE_EXPRESSION($$, COND_LT, $1, $3, @1.first_line, @1.first_column);
      }
      | expr T_LE expr {
        RULE_EXPRESSION($$, COND_LE, $1, $3, @1.first_line, @1.first_column);
      }
      | expr T_GT expr {
        RULE_EXPRESSION($$, COND_GT, $1, $3, @1.first_line, @1.first_column);
      }
      | expr T_GE expr {
        RULE_EXPRESSION($$, COND_GE, $1, $3, @1.first_line, @1.first_column);
      }
      ;

exprlist
      : expr {
        LIST *l = make_node(LIST, filename, @1.first_line, @1.first_column);
        l->list = NULL;
        l->list = dynarray_append_ptr(l->list, $1);
        $$ = (parse_node *)l;
      }
      | exprlist T_COMMA expr {
        LIST *l = (LIST *)$$;
        l->list = dynarray_append_ptr(l->list, $3);
      }
      ;

keyvalue
      : id T_EQU expr {
        EQU *equ = make_node(EQU, filename, @1.first_line, @1.first_column);
        equ->name = (ID *)$1;
        equ->value = (EXPR *)$3;
        $$ = (parse_node *)equ;
      }
      ;

/* key-value list, i.e.: "key1 = expr1, key2 = expr2, ..."*/
kvlist
      : keyvalue {
        LIST *l = make_node(LIST, filename, @1.first_line, @1.first_column);
        l->list = NULL;
        l->list = dynarray_append_ptr(l->list, $1);
        $$ = (parse_node *)l;
      }
      | kvlist T_COMMA keyvalue {
        LIST *l = (LIST *)$$;
        l->list = dynarray_append_ptr(l->list, $3);
      }
      ;

label
      : id T_COLON {
          LABEL *l = make_node(LABEL, filename, @1.first_line, @1.first_column);
          l->name = (ID *)$1;
          *statements = dynarray_append_ptr(*statements, l);
        }

stmt
      : label
      | T_ORG expr {
        ORG *l = make_node(ORG, filename, @2.first_line, @1.first_column);
        l->value = (parse_node *)$2;
        *statements = dynarray_append_ptr(*statements, l);
      }
      | T_END {
        END *l = make_node(END, filename, @1.first_line, @1.first_column);
        *statements = dynarray_append_ptr(*statements, l);
      }
      | T_REPT expr {
        REPT *l = make_node(REPT, filename, @1.first_line, @1.first_column);
        l->count_expr = (EXPR *)$2;
        l->var = NULL;
        *statements = dynarray_append_ptr(*statements, l);
      }
      | T_REPT expr T_COMMA id {
        REPT *l = make_node(REPT, filename, @1.first_line, @1.first_column);
        l->count_expr = (EXPR *)$2;
        l->var = (ID *)$4;
        *statements = dynarray_append_ptr(*statements, l);
      }
      | T_ENDR {
        ENDR *l = make_node(ENDR, filename, @1.first_line, @1.first_column);
        *statements = dynarray_append_ptr(*statements, l);
      }
      | T_PROFILE str {
        PROFILE *l = make_node(PROFILE, filename, @1.first_line, @1.first_column);
        l->name = (LITERAL *)$2;
        *statements = dynarray_append_ptr(*statements, l);
      }
      | T_ENDPROFILE {
        ENDPROFILE *l = make_node(ENDPROFILE, filename, @1.first_line, @1.first_column);
        *statements = dynarray_append_ptr(*statements, l);
      }
      | T_IF expr {
        IF *l = make_node(IF, filename, @1.first_line, @1.first_column);
        l->condition = (EXPR *)$2;
        *statements = dynarray_append_ptr(*statements, l);
      }
      | T_ELSE {
        ELSE *l = make_node(ELSE, filename, @1.first_line, @1.first_column);
        *statements = dynarray_append_ptr(*statements, l);
      }
      | T_ENDIF {
        ENDIF *l = make_node(ENDIF, filename, @1.first_line, @1.first_column);
        *statements = dynarray_append_ptr(*statements, l);
      }
      | id T_COLON T_EQU expr {
        EQU *l = make_node(EQU, filename, @1.first_line, @1.first_column);
        l->name = (ID *)$1;
        l->value = (EXPR *)$4;
        *statements = dynarray_append_ptr(*statements, l);
      }
      | id T_EQU expr {
        EQU *l = make_node(EQU, filename, @1.first_line, @1.first_column);
        l->name = (ID *)$1;
        l->value = (EXPR *)$3;
        *statements = dynarray_append_ptr(*statements, l);
      }
      | T_INCBIN str {
        INCBIN *l = make_node(INCBIN, filename, @1.first_line, @1.first_column);
        l->filename = (LITERAL *)$2;
        *statements = dynarray_append_ptr(*statements, l);
      }
      | T_INCLUDE str {
        LITERAL *lfilename = (LITERAL *)$2;
        parse_include(lfilename->strval, statements);
      }
      | T_DB exprlist {
        DEF *l = make_node(DEF, filename, @1.first_line, @1.first_column);
        l->kind = DEFKIND_DB;
        l->values = (LIST *)$2;
        *statements = dynarray_append_ptr(*statements, l);
      }
      | T_DM exprlist {
        DEF *l = make_node(DEF, filename, @1.first_line, @1.first_column);
        l->kind = DEFKIND_DM;
        l->values = (LIST *)$2;
        *statements = dynarray_append_ptr(*statements, l);
      }
      | T_DW exprlist {
        DEF *l = make_node(DEF, filename, @1.first_line, @1.first_column);
        l->kind = DEFKIND_DW;
        l->values = (LIST *)$2;
        *statements = dynarray_append_ptr(*statements, l);
      }
      | T_DS exprlist {
        DEF *l = make_node(DEF, filename, @1.first_line, @1.first_column);
        l->kind = DEFKIND_DS;
        l->values = (LIST *)$2;
        *statements = dynarray_append_ptr(*statements, l);
      }
      | T_SECTION str {
        SECTION *section = make_node(SECTION, filename, @1.first_line, @1.first_column);
        section->name = (LITERAL *)$2;
        section->params = NULL;
        *statements = dynarray_append_ptr(*statements, section);
      }
      | T_SECTION str kvlist {
        SECTION *section = make_node(SECTION, filename, @1.first_line, @1.first_column);
        section->name = (LITERAL *)$2;
        section->params = (LIST *)$3;
        *statements = dynarray_append_ptr(*statements, section);
      }
      | id exprlist {
        INSTR *l = make_node(INSTR, filename, @1.first_line, @1.first_column);
        l->name = (ID *)$1;
        l->args = (LIST *)$2;
        *statements = dynarray_append_ptr(*statements, l);
      }
      | id {
        INSTR *l = make_node(INSTR, filename, @1.first_line, @1.first_column);
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

static int yyreport_syntax_error(const yypcontext_t *yyctx, void* scanner, dynarray **statements, char *filename, char *source)
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

      char *err_token = get_token_at(source, loc->first_line, loc->first_column, loc->last_column - loc->first_column + 1);
      if (num_expected > 0)
        buffer_append(errmsg, " near '%s'", err_token);
    }
  }

  char *errstr = buffer_dup(errmsg);
  buffer_free(errmsg);

  generic_report_error(ERROR_OUT_LOC | ERROR_OUT_LINE | ERROR_OUT_POS,
    filename, yylloc.first_line, yylloc.first_column,
    errstr);
  return res;
}

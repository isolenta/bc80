%{
  #include <ctype.h>
  #include "common/mmgr.h"
  #include "common/buffer.h"
  #include "rcc/parse_nodes.h"
  #include "rcc/rcc.h"
  #include "rcc/parser.h"

  #include "rc_parser.tab.h"

  #define YY_USER_ACTION rc_lloc.first_line = rc_lloc.last_line = yylineno;

  #undef YY_DECL
  #define YY_DECL int rc_lex (yyscan_t yyscanner, rcc_ctx_t *ctx)

  #define YY_EXTRA_TYPE struct scanner_state *

  extern void rc_error(yyscan_t scanner, rcc_ctx_t *ctx, const char *msg);

  #define NEWLINE do {          \
    yyextra->line_num++;        \
  } while (0)

  buffer *literal_buf;
%}

%option reentrant
%option yylineno
%option prefix="rc_"
%option noyywrap
%x COMMENT
%x STRLITERAL

DECIMAL                   [0-9]
LETTERS                   [a-zA-Z_]
ALNUM                     [a-zA-Z_0-9]
HEXNUM                    [a-fA-F0-9]
HEXPREFIX                 (0[xX])
WHITESPACE                [ \t\v\f]

%%

"\n"                      { NEWLINE; if (yyextra->nl_from_scanner) return '\n'; }

<INITIAL>"/*"             { BEGIN(COMMENT); yyextra->skipped = true; }
<COMMENT>"*/"             { BEGIN(INITIAL); }
<COMMENT>\n               { NEWLINE; }
<COMMENT>.                { }

"//".*                    { /* consume //-comment */ }

"assert"                  { return(ASSERT); }
"break"                   { return(BREAK); }
"case"                    { return(CASE); }
"i8"                      { return(INT8); }
"i16"                     { return(INT16); }
"u8"                      { return(UINT8); }
"u16"                     { return(UINT16); }
"bool"                    { return(BOOL); }
"false"                   { return(FALSE); }
"true"                    { return(TRUE); }
"continue"                { return(CONTINUE); }
"default"                 { return(DEFAULT); }
"do"                      { return(DO); }
"else"                    { return(ELSE); }
"for"                     { return(FOR); }
"goto"                    { return(GOTO); }
"if"                      { return(IF); }
"inline"                  { return(INLINE); }
"return"                  { return(RETURN); }
"sizeof"                  { return(SIZEOF); }
"static"                  { return(STATIC); }
"struct"                  { return(STRUCT); }
"switch"                  { return(SWITCH); }
"void"                    { return(VOID); }
"volatile"                { return(VOLATILE); }
"while"                   { return(WHILE); }
"#include"                { return(PP_INCLUDE); }
"#define"                 { return(PP_DEFINE); }
"#ifdef"                  { return(PP_IFDEF); }
"#ifndef"                 { return(PP_IFNDEF); }
"#else"                   { return(PP_ELSE); }
"#endif"                  { return(PP_ENDIF); }
"#error"                  { return(PP_ERROR); }
"#warning"                { return(PP_WARNING); }
"#line"                   { return(PP_LINE); }

{LETTERS}{ALNUM}*         {
  char *tmp = xmalloc(yyleng + 1);
  memcpy(tmp, yytext, yyleng);
  tmp[yyleng] = '\0';

  rc_lval.strval = tmp;

  return ID;
}

{HEXPREFIX}{HEXNUM}+      {
  rc_lval.ival = parse_int(yytext, yyleng, HEX);
  return INT_LITERAL;
}

{DECIMAL}*                {
  rc_lval.ival = parse_int(yytext, yyleng, DEC);
  return INT_LITERAL;
}

<INITIAL>\"               { BEGIN(STRLITERAL); literal_buf = buffer_init(); }
<STRLITERAL>[^\"] {
  buffer_append_char(literal_buf, *yytext);
}

<STRLITERAL>\"            {
  BEGIN(INITIAL);
  buffer_append_char(literal_buf, 0);
  rc_lval.strval = buffer_dup(literal_buf);
  buffer_free(literal_buf);
  yyextra->string_literal = rc_lval.strval;
  return STRING_LITERAL;
}

">>="                     { return RIGHT_ASSIGN; }
"<<="                     { return LEFT_ASSIGN; }
"+="                      { return ADD_ASSIGN; }
"-="                      { return SUB_ASSIGN; }
"*="                      { return MUL_ASSIGN; }
"/="                      { return DIV_ASSIGN; }
"%="                      { return MOD_ASSIGN; }
"&="                      { return AND_ASSIGN; }
"^="                      { return XOR_ASSIGN; }
"|="                      { return OR_ASSIGN; }
">>"                      { return RIGHT_OP; }
"<<"                      { return LEFT_OP; }
"++"                      { return INC_OP; }
"--"                      { return DEC_OP; }
"->"                      { return PTR_OP; }
"&&"                      { return AND_OP; }
"||"                      { return OR_OP; }
"<="                      { return LE_OP; }
">="                      { return GE_OP; }
"=="                      { return EQ_OP; }
"!="                      { return NE_OP; }
";"                       { return ';'; }
"{"                       { return '{'; }
"}"                       { return '}'; }
","                       { return ','; }
":"                       { return ':'; }
"="                       { return '='; }
"("                       { return '('; }
")"                       { return ')'; }
"["                       { return '['; }
"]"                       { return ']'; }
"."                       { return '.'; }
"&"                       { return '&'; }
"!"                       { return '!'; }
"~"                       { return '~'; }
"-"                       { return '-'; }
"+"                       { return '+'; }
"*"                       { return '*'; }
"/"                       { return '/'; }
"%"                       { return '%'; }
"<"                       { return '<'; }
">"                       { return '>'; }
"^"                       { return '^'; }
"|"                       { return '|'; }
"?"                       { return '?'; }

{WHITESPACE}+             { }

%%

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
    yyextra->pos_num = 1;       \
  } while (0)

  #define ADVANCE_POS do {                                        \
    yyextra->rcc_ctx->current_position.pos = yyextra->pos_num;    \
    yyextra->pos_num += yyleng;                                   \
    yyextra->rcc_ctx->current_position.line = yyextra->line_num;  \
  } while (0)

  #define ESCAPED_INT_LITERAL(_ival) do {  \
    rc_lval.ival = _ival;                  \
    yyextra->int_literal = rc_lval.ival;   \
    return INT_LITERAL;                    \
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

"\n"                      { NEWLINE; if (yyextra->scan_standalone) return '\n'; }

<INITIAL>"/*"             {
  BEGIN(COMMENT);
  yyextra->num_comment_lines = 0;
}

<COMMENT>"*/"             {
  BEGIN(INITIAL);
  return(END_OF_COMMENT);
}

<COMMENT>\n               {
  NEWLINE;
  yyextra->num_comment_lines++;
}

<COMMENT>.                { /* consume anything else inside comment block */ }

"//".*                    { /* consume //-comment */ }

"assert"                  { ADVANCE_POS; return(ASSERT); }
"break"                   { ADVANCE_POS; return(BREAK); }
"case"                    { ADVANCE_POS; return(CASE); }
"i8"                      { ADVANCE_POS; return(INT8); }
"i16"                     { ADVANCE_POS; return(INT16); }
"u8"                      { ADVANCE_POS; return(UINT8); }
"u16"                     { ADVANCE_POS; return(UINT16); }
"bool"                    { ADVANCE_POS; return(BOOL); }
"false"                   { ADVANCE_POS; return(FALSE); }
"true"                    { ADVANCE_POS; return(TRUE); }
"continue"                { ADVANCE_POS; return(CONTINUE); }
"default"                 { ADVANCE_POS; return(DEFAULT); }
"do"                      { ADVANCE_POS; return(DO); }
"else"                    { ADVANCE_POS; return(ELSE); }
"for"                     { ADVANCE_POS; return(FOR); }
"goto"                    { ADVANCE_POS; return(GOTO); }
"if"                      { ADVANCE_POS; return(IF); }
"inline"                  { ADVANCE_POS; return(INLINE); }
"return"                  { ADVANCE_POS; return(RETURN); }
"sizeof"                  { ADVANCE_POS; return(SIZEOF); }
"static"                  { ADVANCE_POS; return(STATIC); }
"struct"                  { ADVANCE_POS; return(STRUCT); }
"switch"                  { ADVANCE_POS; return(SWITCH); }
"void"                    { ADVANCE_POS; return(VOID); }
"volatile"                { ADVANCE_POS; return(VOLATILE); }
"while"                   { ADVANCE_POS; return(WHILE); }
"#include"                { ADVANCE_POS; return(PP_INCLUDE); }
"#define"                 { ADVANCE_POS; return(PP_DEFINE); }
"#ifdef"                  { ADVANCE_POS; return(PP_IFDEF); }
"#ifndef"                 { ADVANCE_POS; return(PP_IFNDEF); }
"#else"                   { ADVANCE_POS; return(PP_ELSE); }
"#endif"                  { ADVANCE_POS; return(PP_ENDIF); }
"#error"                  { ADVANCE_POS; return(PP_ERROR); }
"#warning"                { ADVANCE_POS; return(PP_WARNING); }
"#line"                   { ADVANCE_POS; return(PP_LINE); }

{LETTERS}{ALNUM}*         {
  char *tmp = xmalloc(yyleng + 1);
  memcpy(tmp, yytext, yyleng);
  tmp[yyleng] = '\0';

  rc_lval.strval = tmp;
  ADVANCE_POS;
  return ID;
}

{HEXPREFIX}{HEXNUM}+      {
  rc_lval.ival = parse_int(yytext, yyleng, HEX);
  yyextra->int_literal = rc_lval.ival;
  ADVANCE_POS;
  return INT_LITERAL;
}

{DECIMAL}*                {
  rc_lval.ival = parse_int(yytext, yyleng, DEC);
  yyextra->int_literal = rc_lval.ival;
  ADVANCE_POS;
  return INT_LITERAL;
}

'\"'                        {
  ADVANCE_POS;
  ESCAPED_INT_LITERAL(34);
}

<INITIAL>\"               { BEGIN(STRLITERAL); ADVANCE_POS; literal_buf = buffer_init(); }

<STRLITERAL>\"            {
  BEGIN(INITIAL);
  buffer_append_char(literal_buf, 0);
  rc_lval.strval = buffer_dup(literal_buf);
  buffer_free(literal_buf);
  yyextra->string_literal = rc_lval.strval;
  ADVANCE_POS;
  return STRING_LITERAL;
}

<STRLITERAL>\\\"          {
  ADVANCE_POS;
  buffer_append_char(literal_buf, '\\');
  buffer_append_char(literal_buf, '\"');
}

<STRLITERAL>[^\"]         {
  ADVANCE_POS;
  buffer_append_char(literal_buf, *yytext);
}

'\\n'                       {
  ADVANCE_POS;
  ESCAPED_INT_LITERAL(10);
}

'\\r'                       {
  ADVANCE_POS;
  ESCAPED_INT_LITERAL(13);
}

'\\t'                       {
  ADVANCE_POS;
  ESCAPED_INT_LITERAL(9);
}

'\\\''                      {
  ADVANCE_POS;
  ESCAPED_INT_LITERAL(39);
}

'\\{DECIMAL}'               {
  ADVANCE_POS;
  ESCAPED_INT_LITERAL((int)(yytext[2] - '0'));
}

'{ALNUM}'                 {
  ADVANCE_POS;
  ESCAPED_INT_LITERAL(yytext[1]);
}

"\\\n"                    { ADVANCE_POS; if (yyextra->scan_standalone) return BACKSLASH; }

">>="                     { ADVANCE_POS; return RIGHT_ASSIGN; }
"<<="                     { ADVANCE_POS; return LEFT_ASSIGN; }
"+="                      { ADVANCE_POS; return ADD_ASSIGN; }
"-="                      { ADVANCE_POS; return SUB_ASSIGN; }
"*="                      { ADVANCE_POS; return MUL_ASSIGN; }
"/="                      { ADVANCE_POS; return DIV_ASSIGN; }
"%="                      { ADVANCE_POS; return MOD_ASSIGN; }
"&="                      { ADVANCE_POS; return AND_ASSIGN; }
"^="                      { ADVANCE_POS; return XOR_ASSIGN; }
"|="                      { ADVANCE_POS; return OR_ASSIGN; }
">>"                      { ADVANCE_POS; return RIGHT_OP; }
"<<"                      { ADVANCE_POS; return LEFT_OP; }
"++"                      { ADVANCE_POS; return INC_OP; }
"--"                      { ADVANCE_POS; return DEC_OP; }
"->"                      { ADVANCE_POS; return PTR_OP; }
"&&"                      { ADVANCE_POS; return AND_OP; }
"||"                      { ADVANCE_POS; return OR_OP; }
"<="                      { ADVANCE_POS; return LE_OP; }
">="                      { ADVANCE_POS; return GE_OP; }
"=="                      { ADVANCE_POS; return EQ_OP; }
"!="                      { ADVANCE_POS; return NE_OP; }
";"                       { ADVANCE_POS; return ';'; }
"{"                       { ADVANCE_POS; return '{'; }
"}"                       { ADVANCE_POS; return '}'; }
","                       { ADVANCE_POS; return ','; }
":"                       { ADVANCE_POS; return ':'; }
"="                       { ADVANCE_POS; return '='; }
"("                       { ADVANCE_POS; return '('; }
")"                       { ADVANCE_POS; return ')'; }
"["                       { ADVANCE_POS; return '['; }
"]"                       { ADVANCE_POS; return ']'; }
"."                       { ADVANCE_POS; return '.'; }
"&"                       { ADVANCE_POS; return '&'; }
"!"                       { ADVANCE_POS; return '!'; }
"~"                       { ADVANCE_POS; return '~'; }
"-"                       { ADVANCE_POS; return '-'; }
"+"                       { ADVANCE_POS; return '+'; }
"*"                       { ADVANCE_POS; return '*'; }
"/"                       { ADVANCE_POS; return '/'; }
"%"                       { ADVANCE_POS; return '%'; }
"<"                       { ADVANCE_POS; return '<'; }
">"                       { ADVANCE_POS; return '>'; }
"^"                       { ADVANCE_POS; return '^'; }
"|"                       { ADVANCE_POS; return '|'; }
"?"                       { ADVANCE_POS; return '?'; }

{WHITESPACE}+             {
  ADVANCE_POS;
  if (yyextra->scan_standalone) {
    yyextra->whitespace_str = xmalloc(yyleng + 1);
    memcpy(yyextra->whitespace_str, yytext, yyleng);
    yyextra->whitespace_str[yyleng] = '\0';
    return WHITESPACE;
  }
}

%%

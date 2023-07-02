%{
  #include <ctype.h>
  #include <stdio.h>

  #include "mmgr.h"
  #include "parse.h"
  #include "parser.tab.h"

  #define YY_USER_ACTION yylloc.first_line = yylloc.last_line = yylineno;
  #define YY_DECL int yylex (yyscan_t yyscanner, struct libasm_as_desc_t *desc, jmp_buf *parse_env)
%}

%option noyywrap
%option reentrant
%option yylineno

space         [ \t\f]
newline       [\n\r]
ws            ({space}+)
optws         [[:blank:]]*
comment       ";"[^\n\r]*

alpha   [A-Za-z_]
hexchar [A-Fa-f]
num     [0-9]
id      {alpha}({alpha}|{num})*

decnum  ({num}+|{num}+(d|D))
hexnum1  ({num}|{hexchar})+(H|h)
hexnum2  \$({num}|{hexchar})({num}|{hexchar})*
hexnum3  0x({num}|{hexchar})({num}|{hexchar})*
hexnum  ({hexnum1}|{hexnum2}|{hexnum3})
binnum1  \%(0|1)+
binnum2  0b(0|1)+
binnum3  (0|1)+(b|B)
binnum   ({binnum1}|{binnum2}|{binnum3})
string1  (['][^']*['])
string2  (["][^"]*["])
string  ({string1}|{string2})
intnum  ({decnum}|{hexnum}|{binnum})

%%

{ws}      {}
{comment}      {}
^{optws}{newline}     {}

{newline}     { return T_NL; }

(?i:org)      { return T_ORG; }
(?i:equ)      { return T_EQU; }
(?i:end)      { return T_END; }
(?i:db|defb)  { return T_DB; }
(?i:dm|defm)  { return T_DM; }
(?i:dw|defw)  { return T_DW; }
(?i:ds|defs)  { return T_DS; }
(?i:incbin)   { return T_INCBIN; }
(?i:include)  { return T_INCLUDE; }
(?i:section)  { return T_SECTION; }


{id}      {
  yylval.str = xstrdup(yytext);
  return T_ID;
}

(?i:af)'  {
  /* special rule for shadow register pair af: don't confuse with string literal */
  yylval.str = xstrdup(yytext);
  return T_ID;
}

{string}  {
  char *tmp = xstrdup(yytext);

  tmp[strlen(tmp) - 1] = '\0';
  yylval.str = tmp + 1;

  return T_STR;
}

{decnum}  {
  yylval.ival = parse_integer(desc, yytext, yyleng, 10, 'd', parse_env);
  return T_INT;
}

{hexnum}  {
  yylval.ival = parse_integer(desc, yytext, yyleng, 16, 'h', parse_env);
  return T_INT;
}

{binnum}  {
  yylval.ival = parse_binary(desc, yytext, yyleng, parse_env);
  return T_INT;
}

"("    { return T_LPAR; }
")"    { return T_RPAR; }
"-"    { return T_MINUS; }
"+"    { return T_PLUS; }
"*"    { return T_MUL; }
"/"    { return T_DIV; }
"&"    { return T_AND; }
"|"    { return T_OR; }
"!"    { return T_NOT; }
","    { return T_COMMA; }
":"    { return T_COLON; }
"$"    { return T_DOLLAR; }
"%"    { return T_PERCENT; }
"<<"   { return T_SHL; }
">>"   { return T_SHR; }

.      { return *yytext; }

%%

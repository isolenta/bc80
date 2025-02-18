%{
  #include "asm/parse.h"
  #include "common/mmgr.h"

  #include "parser.tab.h"

  #define YY_USER_ACTION \
    yylloc.first_line = yylloc.last_line = yyextra->line_num;   \
    yylloc.first_column = yyextra->pos_num;                     \
    yylloc.last_column = yylloc.first_column + yyleng - 1;

  #define YY_DECL int yylex (yyscan_t yyscanner, struct libasm_as_desc_t *desc)
  #define YY_EXTRA_TYPE struct as_scanner_state *

  #define ADVANCE_POS do {          \
    yyextra->pos_num += yyleng;     \
  } while (0)
%}

%option noyywrap
%option reentrant
%option yylineno

space         [ \t\f]
newline       [\n\r]
ws            ({space}+)
optws         [[:blank:]]*
comment       ";"[^\n\r]*

alpha         [A-Za-z_.]
hexchar       [A-Fa-f]
num           [0-9]
id            {alpha}({alpha}|{num})*

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

{ws}      { ADVANCE_POS; }

^{optws}{newline}     {
  yyextra->line_num++;
  yyextra->pos_num = 1;
  return T_NL;
}

{newline}     {
  yyextra->line_num++;
  yyextra->pos_num = 1;
  return T_NL;
}

{comment}             {}

(?i:org)        { ADVANCE_POS; return T_ORG; }
(?i:rept)       { ADVANCE_POS; return T_REPT; }
(?i:endr)       { ADVANCE_POS; return T_ENDR; }
(?i:profile)    { ADVANCE_POS; return T_PROFILE; }
(?i:endprofile) { ADVANCE_POS; return T_ENDPROFILE; }
(?i:equ)        { ADVANCE_POS; return T_EQU; }
(?i:end)        { ADVANCE_POS; return T_END; }
(?i:db|defb)    { ADVANCE_POS; return T_DB; }
(?i:dm|defm)    { ADVANCE_POS; return T_DM; }
(?i:dw|defw)    { ADVANCE_POS; return T_DW; }
(?i:ds|defs)    { ADVANCE_POS; return T_DS; }
(?i:incbin)     { ADVANCE_POS; return T_INCBIN; }
(?i:include)    { ADVANCE_POS; return T_INCLUDE; }
(?i:section)    { ADVANCE_POS; return T_SECTION; }


{id}      {
  ADVANCE_POS;
  yylval.str = xstrdup(yytext);
  return T_ID;
}

(?i:af)'  {
  /* special rule for shadow register pair af: don't confuse with string literal */
  ADVANCE_POS;
  yylval.str = xstrdup(yytext);
  return T_ID;
}

{string}  {
  char *tmp = xstrdup(yytext);

  tmp[strlen(tmp) - 1] = '\0';
  yylval.str = tmp + 1;

  ADVANCE_POS;

  return T_STR;
}

{decnum}  {
  yylval.ival = parse_integer(desc, yytext, yyleng, 10, 'd');
  ADVANCE_POS;
  return T_INT;
}

{hexnum}  {
  yylval.ival = parse_integer(desc, yytext, yyleng, 16, 'h');
  ADVANCE_POS;
  return T_INT;
}

{binnum}  {
  yylval.ival = parse_binary(desc, yytext, yyleng);
  ADVANCE_POS;
  return T_INT;
}

"("    {   ADVANCE_POS; return T_LPAR; }
")"    {   ADVANCE_POS; return T_RPAR; }
"-"    {   ADVANCE_POS; return T_MINUS; }
"+"    {   ADVANCE_POS; return T_PLUS; }
"*"    {   ADVANCE_POS; return T_MUL; }
"/"    {   ADVANCE_POS; return T_DIV; }
"&"    {   ADVANCE_POS; return T_AND; }
"|"    {   ADVANCE_POS; return T_OR; }
"!"    {   ADVANCE_POS; return T_NOT; }
","    {   ADVANCE_POS; return T_COMMA; }
":"    {   ADVANCE_POS; return T_COLON; }
"$"    {   ADVANCE_POS; return T_DOLLAR; }
"%"    {   ADVANCE_POS; return T_PERCENT; }
"<<"   {   ADVANCE_POS; return T_SHL; }
">>"   {   ADVANCE_POS; return T_SHR; }

.      {   ADVANCE_POS; return *yytext; }

%%

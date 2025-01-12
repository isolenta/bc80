%{
  #include "rc_parser.tab.h"
  #include "rcc/scanner.h"

  static void comment(yyscan_t yyscanner, struct scanner_state *extra);

  #define YY_DECL int rc_lex (yyscan_t yyscanner)
  #define YY_EXTRA_TYPE struct scanner_state *
  #define YY_USER_ACTION yyextra->line_num = yylineno;

  extern void rc_error(yyscan_t scanner, const char *msg);
%}

%option reentrant
%option yylineno
%option prefix="rc_"
%option noyywrap

DECIMAL                   [0-9]
LETTERS                   [a-zA-Z_]
ALNUM                     [a-zA-Z_0-9]
HEXNUM                    [a-fA-F0-9]
HEXPREFIX                 (0[xX])
WHITESPACE                [ \t\v\n\f]

%%

"\n"                      { yyextra->pos_num = 0; return '\n'; }
"/*"                      { comment(yyscanner, yyextra); }
"//".*                    { /* consume //-comment */ }

"assert"                  { return(ASSERT); }
"break"                   { return(BREAK); }
"case"                    { return(CASE); }
"i8"                      { return(INT8); }
"i16"                     { return(INT16); }
"u8"                      { return(UINT8); }
"u16"                     { return(UINT16); }
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
"#file"                   { return(PP_FILE); }

{LETTERS}{ALNUM}*         { return IDENTIFIER; }

{HEXPREFIX}{HEXNUM}+      { return I_CONSTANT; }
{DECIMAL}*                { return I_CONSTANT; }


(\"([^"\\\n])*\"{WHITESPACE}*)+   { return STRING_LITERAL; }

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

.                         { if (*yytext != '\n') yyextra->pos_num++; }
{WHITESPACE}+             { }

%%

static void comment(yyscan_t yyscanner, struct scanner_state *extra)
{
    int c;

    while ((c = input(yyscanner)) != 0)
    {
      if (c == '*')
      {
        while ((c = input(yyscanner)) == '*')
          ;

        if (c == '/')
          return;

        if (c == 0)
          break;
      }
    }

    extra->skipped = true;

    rc_error(yyscanner, "unterminated comment");
}

%{
    #define YY_DECL int rc_lex (yyscan_t yyscanner)

    #include <stdio.h>
    #include "rc_parser.tab.h"
    #include "rc_lexer.yy.h"

    extern int rc_lex (yyscan_t yyscanner);

    void rc_error(yyscan_t scanner, const char *msg) {
      (void)scanner;
        fflush(stdout);
        fprintf(stderr, "*** %s\n", msg);
    }
%}

%lex-param {void* scanner}
%parse-param {void* scanner}

%locations
%define parse.error verbose
%define api.prefix rc_

%token  ASSERT IDENTIFIER I_CONSTANT STRING_LITERAL SIZEOF
%token  PTR_OP INC_OP DEC_OP LEFT_OP RIGHT_OP LE_OP GE_OP EQ_OP NE_OP
%token  AND_OP OR_OP MUL_ASSIGN DIV_ASSIGN MOD_ASSIGN ADD_ASSIGN
%token  SUB_ASSIGN LEFT_ASSIGN RIGHT_ASSIGN AND_ASSIGN
%token  XOR_ASSIGN OR_ASSIGN

%token  STATIC INLINE VOLATILE
%token  INT8 UINT8 INT16 UINT16 VOID STRUCT

%token  CASE DEFAULT IF ELSE SWITCH WHILE DO FOR GOTO CONTINUE BREAK RETURN

%token  PP_DEFINE PP_IFDEF PP_IFNDEF PP_ELSE PP_ENDIF
%token  PP_INCLUDE PP_ERROR PP_WARNING PP_LINE PP_FILE

%start translation_unit
%%

primary_expression
  : IDENTIFIER
  | constant
  | string
  | '(' expression ')'
  ;

constant
  : I_CONSTANT    /* includes character_constant */
  ;

string
  : STRING_LITERAL
  ;

postfix_expression
  : primary_expression
  | postfix_expression '[' expression ']'
  | postfix_expression '(' ')'
  | postfix_expression '(' argument_expression_list ')'
  | postfix_expression '.' IDENTIFIER
  | postfix_expression PTR_OP IDENTIFIER
  | postfix_expression INC_OP
  | postfix_expression DEC_OP
  | '(' type_name ')' '{' initializer_list '}'
  | '(' type_name ')' '{' initializer_list ',' '}'
  ;

argument_expression_list
  : assignment_expression
  | argument_expression_list ',' assignment_expression
  ;

unary_expression
  : postfix_expression
  | INC_OP unary_expression
  | DEC_OP unary_expression
  | unary_operator cast_expression
  | SIZEOF '(' type_name ')'
  ;

unary_operator
  : '&'
  | '*'
  | '+'
  | '-'
  | '~'
  | '!'
  ;

cast_expression
  : unary_expression
  | '(' type_name ')' cast_expression
  ;

multiplicative_expression
  : cast_expression
  | multiplicative_expression '*' cast_expression
  | multiplicative_expression '/' cast_expression
  | multiplicative_expression '%' cast_expression
  ;

additive_expression
  : multiplicative_expression
  | additive_expression '+' multiplicative_expression
  | additive_expression '-' multiplicative_expression
  ;

shift_expression
  : additive_expression
  | shift_expression LEFT_OP additive_expression
  | shift_expression RIGHT_OP additive_expression
  ;

relational_expression
  : shift_expression
  | relational_expression '<' shift_expression
  | relational_expression '>' shift_expression
  | relational_expression LE_OP shift_expression
  | relational_expression GE_OP shift_expression
  ;

equality_expression
  : relational_expression
  | equality_expression EQ_OP relational_expression
  | equality_expression NE_OP relational_expression
  ;

and_expression
  : equality_expression
  | and_expression '&' equality_expression
  ;

exclusive_or_expression
  : and_expression
  | exclusive_or_expression '^' and_expression
  ;

inclusive_or_expression
  : exclusive_or_expression
  | inclusive_or_expression '|' exclusive_or_expression
  ;

logical_and_expression
  : inclusive_or_expression
  | logical_and_expression AND_OP inclusive_or_expression
  ;

logical_or_expression
  : logical_and_expression
  | logical_or_expression OR_OP logical_and_expression
  ;

conditional_expression
  : logical_or_expression
  | logical_or_expression '?' expression ':' conditional_expression
  ;

assignment_expression
  : conditional_expression
  | unary_expression assignment_operator assignment_expression
  ;

assignment_operator
  : '='
  | MUL_ASSIGN
  | DIV_ASSIGN
  | MOD_ASSIGN
  | ADD_ASSIGN
  | SUB_ASSIGN
  | LEFT_ASSIGN
  | RIGHT_ASSIGN
  | AND_ASSIGN
  | XOR_ASSIGN
  | OR_ASSIGN
  ;

expression
  : assignment_expression
  | expression ',' assignment_expression
  ;

constant_expression
  : conditional_expression  /* with constraints */
  ;

declaration
  : declaration_specifiers ';'
  | declaration_specifiers init_declarator_list ';'
  | assert_declaration
  | pp_position
  ;

pp_position
  : PP_LINE I_CONSTANT
  | PP_FILE STRING_LITERAL

declaration_specifiers
  : storage_class_specifier declaration_specifiers
  | storage_class_specifier
  | type_specifier declaration_specifiers
  | type_specifier
  | function_specifier declaration_specifiers
  | function_specifier
  ;

init_declarator_list
  : init_declarator
  | init_declarator_list ',' init_declarator
  ;

init_declarator
  : declarator '=' initializer
  | declarator
  ;

storage_class_specifier
  : STATIC
  ;

type_specifier
  : VOID
  | INT8
  | UINT8
  | INT16
  | UINT16
  | struct_specifier
  ;

struct_specifier
  : STRUCT IDENTIFIER '{' struct_declaration_list '}'
  | STRUCT IDENTIFIER
  ;

struct_declaration_list
  : struct_declaration
  | struct_declaration_list struct_declaration
  ;

struct_declaration
  : specifier_list struct_declarator_list ';'
  | assert_declaration
  ;

specifier_list
  : type_specifier specifier_list
  | type_specifier
  ;

struct_declarator_list
  : struct_declarator
  | struct_declarator_list ',' struct_declarator
  ;

struct_declarator
  : declarator
  ;

function_specifier
  : INLINE
  ;

declarator
  : pointer direct_declarator
  | direct_declarator
  ;

direct_declarator
  : IDENTIFIER
  | '(' declarator ')'
  | direct_declarator '[' ']'
  | direct_declarator '[' '*' ']'
  | direct_declarator '[' STATIC assignment_expression ']'
  | direct_declarator '[' assignment_expression ']'
  | direct_declarator '(' parameter_list ')'
  | direct_declarator '(' ')'
  | direct_declarator '(' identifier_list ')'
  ;

pointer
  : '*' pointer
  | '*'
  ;

parameter_list
  : parameter_declaration
  | parameter_list ',' parameter_declaration
  ;

parameter_declaration
  : declaration_specifiers declarator
  | declaration_specifiers abstract_declarator
  | declaration_specifiers
  ;

identifier_list
  : IDENTIFIER
  | identifier_list ',' IDENTIFIER
  ;

type_name
  : specifier_list abstract_declarator
  | specifier_list
  ;

abstract_declarator
  : pointer direct_abstract_declarator
  | pointer
  | direct_abstract_declarator
  ;

direct_abstract_declarator
  : '(' abstract_declarator ')'
  | '[' ']'
  | '[' '*' ']'
  | '[' STATIC assignment_expression ']'
  | '[' assignment_expression ']'
  | direct_abstract_declarator '[' ']'
  | direct_abstract_declarator '[' '*' ']'
  | direct_abstract_declarator '[' STATIC assignment_expression ']'
  | direct_abstract_declarator '[' assignment_expression ']'
  | '(' ')'
  | '(' parameter_list ')'
  | direct_abstract_declarator '(' ')'
  | direct_abstract_declarator '(' parameter_list ')'
  ;

initializer
  : '{' initializer_list '}'
  | '{' initializer_list ',' '}'
  | assignment_expression
  ;

initializer_list
  : designation initializer
  | initializer
  | initializer_list ',' designation initializer
  | initializer_list ',' initializer
  ;

designation
  : designator_list '='
  ;

designator_list
  : designator
  | designator_list designator
  ;

designator
  : '[' constant_expression ']'
  | '.' IDENTIFIER
  ;

assert_declaration
  : ASSERT '(' constant_expression ',' STRING_LITERAL ')' ';'
  ;

statement
  : labeled_statement
  | compound_statement
  | expression_statement
  | selection_statement
  | iteration_statement
  | jump_statement
  ;

labeled_statement
  : IDENTIFIER ':' statement
  | CASE constant_expression ':' statement
  | DEFAULT ':' statement
  ;

compound_statement
  : '{' '}'
  | '{'  block_item_list '}'
  ;

block_item_list
  : block_item
  | block_item_list block_item
  ;

block_item
  : declaration
  | statement
  ;

expression_statement
  : ';'
  | expression ';'
  ;

selection_statement
  : IF '(' expression ')' statement ELSE statement
  | IF '(' expression ')' statement
  | SWITCH '(' expression ')' statement
  ;

iteration_statement
  : WHILE '(' expression ')' statement
  | DO statement WHILE '(' expression ')' ';'
  | FOR '(' expression_statement expression_statement ')' statement
  | FOR '(' expression_statement expression_statement expression ')' statement
  | FOR '(' declaration expression_statement ')' statement
  | FOR '(' declaration expression_statement expression ')' statement
  ;

jump_statement
  : GOTO IDENTIFIER ';'
  | CONTINUE ';'
  | BREAK ';'
  | RETURN ';'
  | RETURN expression ';'
  ;

translation_unit
  : external_declaration
  | translation_unit external_declaration
  ;

external_declaration
  : function_definition
  | declaration
  ;

function_definition
  : declaration_specifiers declarator declaration_list compound_statement
  | declaration_specifiers declarator compound_statement
  ;

declaration_list
  : declaration
  | declaration_list declaration
  ;

%%

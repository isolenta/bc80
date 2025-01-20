%{
    /* based on ANSI C Yacc grammar: https://www.quut.com/c/ANSI-C-grammar-y.html */

    #define YY_DECL int rc_lex (yyscan_t yyscanner)

    #include <stdio.h>
    #include <libgen.h>

    #include "common/mmgr.h"
    #include "rcc/parse_nodes.h"
    #include "rcc/rcc.h"

    #include "rc_parser.tab.h"
    #include "rc_lexer.yy.h"

    extern int rc_lex (yyscan_t yyscanner, rcc_ctx_t *ctx);

    void rc_error(yyscan_t scanner, rcc_ctx_t *ctx, const char *msg) {
      char *filename = ctx->in_filename;
      int adjusted_line = ctx->scanner_state.line_num + ctx->parser_state.last_pp_line - ctx->parser_state.last_actual_line;
      int actual_line = get_actual_position(ctx, ctx->scanner_state.line_num, &filename);
      generic_report_error(basename(filename), actual_line, ctx->scanner_state.pos_num, "%s", msg);
    }

    #define RULE_UNARY_EXPRESSION(target, _kind, op, _line) do {            \
      EXPRESSION *expr = (EXPRESSION *)CreateParseNode(EXPRESSION, _line);  \
      expr->kind = _kind;                                                   \
      AddChild((ParseNode *)expr, op);                                      \
      target = (ParseNode *)expr;                                           \
    } while(0)

    #define RULE_BINARY_EXPRESSION(target, _kind, op1, op2, _line) do {     \
      EXPRESSION *expr = (EXPRESSION *)CreateParseNode(EXPRESSION, _line);  \
      expr->kind = _kind;                                                   \
      AddChild((ParseNode *)expr, op1);                                     \
      AddChild((ParseNode *)expr, op2);                                     \
      target = (ParseNode *)expr;                                           \
    } while(0)

    #define RULE_SPECIFIER(target, _kind, _line) do {       \
      SPECIFIER *node = CreateParseNode(SPECIFIER, _line);  \
      node->kind = _kind;                                   \
      node->name = NULL;                                    \
      target = (ParseNode *)node;                           \
    } while(0)
%}

%lex-param {void* scanner}{rcc_ctx_t *ctx}
%parse-param {void* scanner}{rcc_ctx_t *ctx}

%union {
  char *strval;
  int ival;
  ParseNode *node;
  ExpressionKind expr_kind;
}

%locations
%define parse.error verbose
%define api.prefix {rc_}

%token  <ival> INT_LITERAL
%token  <strval> STRING_LITERAL ID
%token  ASSERT SIZEOF END_OF_COMMENT WHITESPACE BACKSLASH
%token  PTR_OP INC_OP DEC_OP LEFT_OP RIGHT_OP LE_OP GE_OP EQ_OP NE_OP
%token  AND_OP OR_OP MUL_ASSIGN DIV_ASSIGN MOD_ASSIGN ADD_ASSIGN
%token  SUB_ASSIGN LEFT_ASSIGN RIGHT_ASSIGN AND_ASSIGN
%token  XOR_ASSIGN OR_ASSIGN

%token  STATIC INLINE VOLATILE
%token  INT8 UINT8 INT16 UINT16 VOID STRUCT BOOL FALSE TRUE

%token  CASE DEFAULT IF ELSE SWITCH WHILE DO FOR GOTO CONTINUE BREAK RETURN

%token  PP_DEFINE PP_IFDEF PP_IFNDEF PP_ELSE PP_ENDIF
%token  PP_INCLUDE PP_ERROR PP_WARNING PP_LINE

%type <node> constant string identifier postfix_expression unary_expression
%type <node> multiplicative_expression additive_expression shift_expression
%type <node> relational_expression equality_expression and_expression
%type <node> inclusive_or_expression exclusive_or_expression logical_and_expression logical_or_expression
%type <node> assignment_expression init_declarator_list argument_expression_list
%type <node> expression cast_expression init_declarator primary_expression constant_expression
%type <node> statement labeled_statement compound_statement expression_statement
%type <node> selection_statement iteration_statement jump_statement
%type <node> declaration assert_declaration external_declaration position_declaration
%type <node> designator designator_list initializer initializer_list
%type <node> translation_unit block_item block_item_list
%type <node> storage_class_specifier function_specifier type_specifier struct_specifier
%type <node> declaration_specifiers function_definition declarator direct_declarator
%type <node> struct_declaration struct_declaration_list struct_declarator struct_declarator_list
%type <node> pointer parameter_list parameter_declaration identifier_list type_name

%type <expr_kind> unary_operator assignment_operator

%start translation_unit
%%

primary_expression
  : identifier  { $$ = $1; }
  | constant { $$ = $1; }
  | string { $$ = $1; }
  | '(' expression ')' { $$ = $2; }
  ;

constant
  : INT_LITERAL    /* includes character_constant */
  {
    LITERAL *node = CreateParseNode(LITERAL, @1.first_line);
    node->kind = LITINT;
    node->ival = $1;
    $$ = (ParseNode *)node;
  }
  | FALSE
  {
    LITERAL *node = CreateParseNode(LITERAL, @1.first_line);
    node->kind = LITBOOL;
    node->bval = false;
    $$ = (ParseNode *)node;
  }
  | TRUE
  {
    LITERAL *node = CreateParseNode(LITERAL, @1.first_line);
    node->kind = LITBOOL;
    node->bval = true;
    $$ = (ParseNode *)node;
  }
  ;

string
  : STRING_LITERAL
  {
    LITERAL *node = CreateParseNode(LITERAL, @1.first_line);
    node->kind = LITSTR;
    node->strval = $1;
    $$ = (ParseNode *)node;
  };

identifier
  : ID
  {
    IDENTIFIER *node = CreateParseNode(IDENTIFIER, @1.first_line);
    node->name = $1;
    $$ = (ParseNode *)node;
  };

postfix_expression
  : primary_expression { $$ = $1; }
  | postfix_expression '[' expression ']' {
    RULE_BINARY_EXPRESSION($$, PFX_EXPR_ARRAY, $1, $3, @1.first_line);
  }
  | postfix_expression '(' ')' {
    RULE_UNARY_EXPRESSION($$, PFX_EXPR_CALL, $1, @1.first_line);
  }
  | postfix_expression '(' argument_expression_list ')' {
    RULE_BINARY_EXPRESSION($$, PFX_EXPR_CALL, $1, $3, @1.first_line);
  }
  | postfix_expression '.' identifier {
    RULE_BINARY_EXPRESSION($$, PFX_EXPR_FIELD_ACCESS, $1, $3, @1.first_line);
  }
  | postfix_expression PTR_OP identifier {
    RULE_BINARY_EXPRESSION($$, PFX_EXPR_POINTER_OP, $1, $3, @1.first_line);
  }
  | postfix_expression INC_OP {
    RULE_UNARY_EXPRESSION($$, PFX_EXPR_INCREMENT, $1, @1.first_line);
  }
  | postfix_expression DEC_OP {
    RULE_UNARY_EXPRESSION($$, PFX_EXPR_DECREMENT, $1, @1.first_line);
  }
  ;

argument_expression_list
  : assignment_expression {
    ParseNode *node = CreatePrimitiveParseNode(ARG_EXPR_LIST, @1.first_line);
    AddChild(node, $1);
    $$ = node;
  }
  | argument_expression_list ',' assignment_expression {
    AddChild($1, $3);
  }
  ;

unary_expression
  : postfix_expression { $$ = $1; }
  | INC_OP unary_expression {
    RULE_UNARY_EXPRESSION($$, PRE_EXPR_INCREMENT, $2, @1.first_line);
  }
  | DEC_OP unary_expression {
    RULE_UNARY_EXPRESSION($$, PRE_EXPR_DECREMENT, $2, @1.first_line);
  }
  | unary_operator cast_expression {
    RULE_UNARY_EXPRESSION($$, $1, $2, @1.first_line);
  }
  | SIZEOF '(' type_name ')' {
    RULE_UNARY_EXPRESSION($$, EXPR_SIZEOF, $3, @1.first_line);
  }
  ;

unary_operator
  : '&' { $$ = UNARY_REF; }
  | '*' { $$ = UNARY_DEREF; }
  | '+' { $$ = UNARY_PLUS; }
  | '-' { $$ = UNARY_MINUS; }
  | '~' { $$ = UNARY_INV; }
  | '!' { $$ = UNARY_NOT; }
  ;

cast_expression
  : unary_expression { $$ = $1; }
  | '(' type_name ')' cast_expression {
    CAST *node = CreateParseNode(CAST, @1.first_line);
    node->cast_to = $2;
    AddChild(node, $4);
    $$ = (ParseNode *)node;
  }
  ;

multiplicative_expression
  : cast_expression { $$ = $1; }
  | multiplicative_expression '*' cast_expression {
    RULE_BINARY_EXPRESSION($$, BIN_EXPR_MUL, $1, $3, @1.first_line);
  }
  | multiplicative_expression '/' cast_expression {
    RULE_BINARY_EXPRESSION($$, BIN_EXPR_DIV, $1, $3, @1.first_line);
  }
  | multiplicative_expression '%' cast_expression {
    RULE_BINARY_EXPRESSION($$, BIN_EXPR_MOD, $1, $3, @1.first_line);
  }
  ;

additive_expression
  : multiplicative_expression { $$ = $1; }
  | additive_expression '+' multiplicative_expression {
    RULE_BINARY_EXPRESSION($$, BIN_EXPR_ADD, $1, $3, @1.first_line);
  }
  | additive_expression '-' multiplicative_expression {
    RULE_BINARY_EXPRESSION($$, BIN_EXPR_SUB, $1, $3, @1.first_line);
  }
  ;

shift_expression
  : additive_expression { $$ = $1; }
  | shift_expression LEFT_OP additive_expression {
    RULE_BINARY_EXPRESSION($$, BIN_EXPR_SHL, $1, $3, @1.first_line);
  }
  | shift_expression RIGHT_OP additive_expression {
    RULE_BINARY_EXPRESSION($$, BIN_EXPR_SHR, $1, $3, @1.first_line);
  }
  ;

relational_expression
  : shift_expression { $$ = $1; }
  | relational_expression '<' shift_expression {
    RULE_BINARY_EXPRESSION($$, BIN_EXPR_LT, $1, $3, @1.first_line);
  }
  | relational_expression '>' shift_expression {
    RULE_BINARY_EXPRESSION($$, BIN_EXPR_GT, $1, $3, @1.first_line);
  }
  | relational_expression LE_OP shift_expression {
    RULE_BINARY_EXPRESSION($$, BIN_EXPR_LE, $1, $3, @1.first_line);
  }
  | relational_expression GE_OP shift_expression {
    RULE_BINARY_EXPRESSION($$, BIN_EXPR_GE, $1, $3, @1.first_line);
  }
  ;

equality_expression
  : relational_expression { $$ = $1; }
  | equality_expression EQ_OP relational_expression {
    RULE_BINARY_EXPRESSION($$, BIN_EXPR_EQ, $1, $3, @1.first_line);
  }
  | equality_expression NE_OP relational_expression {
    RULE_BINARY_EXPRESSION($$, BIN_EXPR_NE, $1, $3, @1.first_line);
  }
  ;

and_expression
  : equality_expression { $$ = $1; }
  | and_expression '&' equality_expression {
    RULE_BINARY_EXPRESSION($$, BIN_EXPR_BINARY_AND, $1, $3, @1.first_line);
  }
  ;

exclusive_or_expression
  : and_expression { $$ = $1; }
  | exclusive_or_expression '^' and_expression {
    RULE_BINARY_EXPRESSION($$, BIN_EXPR_LOGICAL_XOR, $1, $3, @1.first_line);
  }
  ;

inclusive_or_expression
  : exclusive_or_expression { $$ = $1; }
  | inclusive_or_expression '|' exclusive_or_expression {
    RULE_BINARY_EXPRESSION($$, BIN_EXPR_BINARY_OR, $1, $3, @1.first_line);
  }
  ;

logical_and_expression
  : inclusive_or_expression { $$ = $1; }
  | logical_and_expression AND_OP inclusive_or_expression {
    RULE_BINARY_EXPRESSION($$, BIN_EXPR_LOGICAL_AND, $1, $3, @1.first_line);
  }
  ;

logical_or_expression
  : logical_and_expression { $$ = $1; }
  | logical_or_expression OR_OP logical_and_expression {
    RULE_BINARY_EXPRESSION($$, BIN_EXPR_LOGICAL_OR, $1, $3, @1.first_line);
  }
  ;

assignment_expression
  : logical_or_expression { $$ = $1; }
  | unary_expression assignment_operator assignment_expression {
    RULE_BINARY_EXPRESSION($$, $2, $1, $3, @1.first_line);
  }
  ;

assignment_operator
  : '='           { $$ = EXPR_ASSIGN; }
  | MUL_ASSIGN    { $$ = EXPR_MUL_ASSIGN;}
  | DIV_ASSIGN    { $$ = EXPR_DIV_ASSIGN;}
  | MOD_ASSIGN    { $$ = EXPR_MOD_ASSIGN;}
  | ADD_ASSIGN    { $$ = EXPR_ADD_ASSIGN;}
  | SUB_ASSIGN    { $$ = EXPR_SUB_ASSIGN;}
  | LEFT_ASSIGN   { $$ = EXPR_LEFT_ASSIGN;}
  | RIGHT_ASSIGN  { $$ = EXPR_RIGHT_ASSIGN;}
  | AND_ASSIGN    { $$ = EXPR_AND_ASSIGN;}
  | XOR_ASSIGN    { $$ = EXPR_XOR_ASSIGN;}
  | OR_ASSIGN     { $$ = EXPR_OR_ASSIGN;}
  ;

expression
  : assignment_expression {
    ParseNode *node = CreatePrimitiveParseNode(EXPR_LIST, @1.first_line);
    AddChild(node, $1);
    $$ = node;
  }
  | expression ',' assignment_expression {
    AddChild($1, $3);
  }
  ;

constant_expression
  : logical_or_expression { $$ = $1; }
  ;

declaration
  : declaration_specifiers ';' { $$ = $1; }
  | declaration_specifiers init_declarator_list ';' {
    ParseNode *specifiers = $1;
    AddChild(specifiers, $2);
    $$ = specifiers;
  }
  | assert_declaration { $$ = $1; }
  | position_declaration { $$ = $1; }
  ;

position_declaration
  : PP_LINE STRING_LITERAL INT_LITERAL {
    POSITION_DECL *node = CreateParseNode(POSITION_DECL, @1.first_line);
    node->file = $2;
    node->line = $3;
    ctx->parser_state.last_pp_line = $3;
    ctx->parser_state.last_actual_line = @1.first_line;
    $$ = (ParseNode *)node;
  }
  ;

declaration_specifiers
  : storage_class_specifier declaration_specifiers {
    AddChild($1, $2);
  }
  | storage_class_specifier {
    ParseNode *node = CreatePrimitiveParseNode(DECL_SPEC_LIST, @1.first_line);
    AddChild(node, $1);
    $$ = node;
  }
  | type_specifier declaration_specifiers {
    AddChild($1, $2);
    $$ = $1;
  }
  | type_specifier { $$ = $1; }
  | function_specifier declaration_specifiers {
    AddChild($1, $2);
  }
  | function_specifier {
    ParseNode *node = CreatePrimitiveParseNode(DECL_SPEC_LIST, @1.first_line);
    AddChild(node, $1);
    $$ = node;
  }
  ;

init_declarator_list
  : init_declarator {
    ParseNode *node = CreatePrimitiveParseNode(INIT_DECLARATOR_LIST, @1.first_line);
    AddChild(node, $1);
    $$ = node;
  }
  | init_declarator_list ',' init_declarator {
    AddChild($1, $3);
  }
  ;

init_declarator
  : declarator '=' initializer {
    AddChild($1, $3);
    $$ = $1;
  }
  | declarator { $$ = $1; }
  ;

storage_class_specifier
  : STATIC {
    RULE_SPECIFIER($$, SPEC_STATIC, @1.first_line);
  }
  ;

type_specifier
  : VOID {
    RULE_SPECIFIER($$, SPEC_VOID, @1.first_line);
  }
  | INT8 {
    RULE_SPECIFIER($$, SPEC_INT8, @1.first_line);
  }
  | UINT8 {
    RULE_SPECIFIER($$, SPEC_UINT8, @1.first_line);
  }
  | INT16 {
    RULE_SPECIFIER($$, SPEC_INT16, @1.first_line);
  }
  | UINT16 {
    RULE_SPECIFIER($$, SPEC_UINT16, @1.first_line);
  }
  | BOOL {
    RULE_SPECIFIER($$, SPEC_BOOL, @1.first_line);
  }
  | struct_specifier { $$ = $1; }
  ;

struct_specifier
  : STRUCT identifier '{' struct_declaration_list '}' {
    SPECIFIER *node = CreateParseNode(SPECIFIER, @1.first_line);
    node->kind = SPEC_STRUCT;
    node->name = (IDENTIFIER *)$2;
    AddChild(node, $4);
    $$ = (ParseNode *)node;
  }
  | STRUCT identifier {
    SPECIFIER *node = CreateParseNode(SPECIFIER, @1.first_line);
    node->kind = SPEC_STRUCT;
    node->name = (IDENTIFIER *)$2;
    $$ = (ParseNode *)node;
  }
  ;

struct_declaration_list
  : struct_declaration {
    ParseNode *list = CreatePrimitiveParseNode(STRUCT_DECL_LIST, @1.first_line);
    AddChild(list, $1);
    $$ = list;
  }
  | struct_declaration_list struct_declaration {
    AddChild($1, $2);
  }
  ;

struct_declaration
  : type_specifier struct_declarator_list ';' {
    AddChild($1, $2);
    $$ = $1;
  }
  | assert_declaration { $$ = $1; }
  ;

struct_declarator_list
  : struct_declarator {
    ParseNode *list = CreatePrimitiveParseNode(STRUCT_DECLARATOR_LIST, @1.first_line);
    AddChild(list, $1);
    $$ = list;
  }
  | struct_declarator_list ',' struct_declarator {
    AddChild($1, $3);
  }
  ;

struct_declarator
  : declarator { $$ = $1; }
  ;

function_specifier
  : INLINE  {
    RULE_SPECIFIER($$, SPEC_INLINE, @1.first_line);
  }
  ;

declarator
  : pointer direct_declarator {
    AddChild($1, $2);
    $$ = $1;
  }
  | direct_declarator { $$ = $1; }
  ;

direct_declarator
  : identifier { $$ = $1; }
  | '(' declarator ')' { $$ = $2; }
  | direct_declarator '[' assignment_expression ']' {
    RULE_BINARY_EXPRESSION($$, PFX_EXPR_ARRAY, $1, $3, @1.first_line);
  }
  | direct_declarator '(' parameter_list ')' {
    RULE_BINARY_EXPRESSION($$, PFX_EXPR_CALL, $1, $3, @1.first_line);
  }
  | direct_declarator '(' ')' {
    RULE_UNARY_EXPRESSION($$, PFX_EXPR_CALL, $1, @1.first_line);
  }
  | direct_declarator '(' identifier_list ')' {
    RULE_BINARY_EXPRESSION($$, PFX_EXPR_CALL, $1, $3, @1.first_line);
  }
  ;

pointer
  : '*' pointer {
    ParseNode *node = CreatePrimitiveParseNode(POINTER, @1.first_line);
    AddChild(node, $2);
    $$ = node;
  }
  | '*' {
    $$ = CreatePrimitiveParseNode(POINTER, @1.first_line);
  }
  ;

parameter_list
  : parameter_declaration {
    ParseNode *list = CreatePrimitiveParseNode(PARAMETER_LIST, @1.first_line);
    AddChild(list, $1);
    $$ = list;
  }
  | parameter_list ',' parameter_declaration {
    AddChild($1, $3);
  }
  ;

parameter_declaration
  : declaration_specifiers declarator {
    AddChild($1, $2);
    $$ = $1;
  }
  | declaration_specifiers { $$ = $1; }
  ;

identifier_list
  : identifier {
    ParseNode *list = CreatePrimitiveParseNode(IDENTIFIER_LIST, @1.first_line);
    AddChild(list, $1);
    $$ = list;
  }
  | identifier_list ',' identifier {
    AddChild($1, $3);
  }
  ;

type_name
  : type_specifier pointer {
    ParseNode *deepest_pointer = $2;

    while (1) {
      if (dynarray_length(deepest_pointer->childs->list) > 0) {
        ParseNode *child = (ParseNode *)dinitial(deepest_pointer->childs->list);
        if (child->type == T_POINTER) {
          deepest_pointer = child;
          continue;
        }
      }
      break;
    }

    AddChild(deepest_pointer, $1);
    $$ = $2;
  }
  | type_specifier { $$ = $1; }
  ;

initializer
  : '{' initializer_list '}' { $$ = $2; }
  | '{' initializer_list ',' '}' { $$ = $2; }
  | assignment_expression { $$ = $1; }
  ;

initializer_list
  : designator_list '=' initializer {
    ParseNode *list = CreatePrimitiveParseNode(INITIALIZER_LIST, @1.first_line);
    ParseNode *tuple = CreatePrimitiveParseNode(TUPLE, @1.first_line);
    AddChild(tuple, $1); // designator
    AddChild(tuple, $3); // initializer
    AddChild(list, tuple);
    $$ = list;
  }
  | initializer {
    ParseNode *list = CreatePrimitiveParseNode(INITIALIZER_LIST, @1.first_line);
    AddChild(list, $1);
    $$ = list;
  }
  | initializer_list ',' designator_list '=' initializer {
    ParseNode *tuple = CreatePrimitiveParseNode(TUPLE, @1.first_line);
    AddChild(tuple, $3); // designator
    AddChild(tuple, $5); // initializer
    AddChild($1, tuple);
  }
  | initializer_list ',' initializer {
    AddChild($1, $3);
  }
  ;

designator_list
  : designator {
    ParseNode *node = CreatePrimitiveParseNode(DESIGNATOR_LIST, @1.first_line);
    AddChild(node, $1);
    $$ = node;
  }
  | designator_list designator {
    AddChild($1, $2);
  }
  ;

designator
  : '[' constant_expression ']' {
    DESIGNATOR *node = CreateParseNode(DESIGNATOR, @1.first_line);
    node->kind = DESIGNATOR_ARRAY;
    AddChild(node, $2);
    $$ = (ParseNode *)node;
  }
  | '.' identifier {
    DESIGNATOR *node = CreateParseNode(DESIGNATOR, @1.first_line);
    node->kind = DESIGNATOR_FIELD;
    AddChild(node, $2);
    $$ = (ParseNode *)node;
  }
  ;

assert_declaration
  : ASSERT '(' constant_expression ',' string ')' ';' {
    ASSERT_DECL *node = CreateParseNode(ASSERT_DECL, @1.first_line);
    node->expr = (EXPRESSION *)$3;
    node->message = (LITERAL *)$5;
    $$ = (ParseNode *)node;
  }
  ;

statement
  : labeled_statement { $$ = $1; }
  | compound_statement { $$ = $1; }
  | expression_statement { $$ = $1; }
  | selection_statement { $$ = $1; }
  | iteration_statement { $$ = $1; }
  | jump_statement { $$ = $1; }
  ;

labeled_statement
  : identifier ':' statement {
    LABELED_STMT *node = CreateParseNode(LABELED_STMT, @1.first_line);
    node->kind = LABELED_LABEL;
    node->name = $1;
    AddChild(node, $3);
    $$ = (ParseNode *)node;
  }
  | CASE constant_expression ':' statement {
    LABELED_STMT *node = CreateParseNode(LABELED_STMT, @1.first_line);
    node->kind = LABELED_CASE;
    node->name = $2;
    AddChild(node, $4);
    $$ = (ParseNode *)node;
  }
  | DEFAULT ':' statement {
    LABELED_STMT *node = CreateParseNode(LABELED_STMT, @1.first_line);
    node->kind = LABELED_DEFAULT;
    node->name = NULL;
    AddChild(node, $3);
    $$ = (ParseNode *)node;
  }
  ;

compound_statement
  : '{' '}' { $$ = (ParseNode *)CreateParseNode(LIST, @1.first_line); }
  | '{'  block_item_list '}' { $$ = $2; }
  ;

block_item_list
  : block_item {
    ParseNode *node = CreatePrimitiveParseNode(BLOCK_LIST, @1.first_line);
    AddChild(node, $1);
    $$ = node;
  }
  | block_item_list block_item {
    AddChild($1, $2);
  }
  ;

block_item
  : declaration { $$ = $1; }
  | statement { $$ = $1; }
  ;

expression_statement
  : ';' { $$ = (ParseNode *)NULL; }
  | expression ';' { $$ = $1; }
  ;

selection_statement
  : IF '(' expression ')' statement ELSE statement {
    SELECT_STMT *node = CreateParseNode(SELECT_STMT, @1.first_line);
    node->kind = SELECT_IF;
    node->expr = $3;
    AddChild(node, $5);
    AddChild(node, $7);
    $$ = (ParseNode *)node;
  }
  | IF '(' expression ')' statement {
    SELECT_STMT *node = CreateParseNode(SELECT_STMT, @1.first_line);
    node->kind = SELECT_IF;
    node->expr = $3;
    AddChild(node, $5);
    $$ = (ParseNode *)node;
  }
  | SWITCH '(' expression ')' statement {
    SELECT_STMT *node = CreateParseNode(SELECT_STMT, @1.first_line);
    node->kind = SELECT_SWITCH;
    node->expr = $3;
    AddChild(node, $5);
    $$ = (ParseNode *)node;
  }
  ;

iteration_statement
  : WHILE '(' expression ')' statement {
    ITERATION_STMT *node = CreateParseNode(ITERATION_STMT, @1.first_line);
    node->kind = ITERATION_WHILE;
    node->init = NULL;
    node->step = NULL;
    node->cond = $3;
    AddChild(node, $5);
    $$ = (ParseNode *)node;
  }
  | DO statement WHILE '(' expression ')' ';' {
    ITERATION_STMT *node = CreateParseNode(ITERATION_STMT, @1.first_line);
    node->kind = ITERATION_DO;
    node->init = NULL;
    node->step = NULL;
    node->cond = $5;
    AddChild(node, $2);
    $$ = (ParseNode *)node;
  }
  | FOR '(' expression_statement expression_statement ')' statement {
    ITERATION_STMT *node = CreateParseNode(ITERATION_STMT, @1.first_line);
    node->kind = ITERATION_FOR;
    node->init = $3;
    node->cond = $4;
    node->step = NULL;
    AddChild(node, $6);
    $$ = (ParseNode *)node;
  }
  | FOR '(' declaration expression_statement ')' statement {
    ITERATION_STMT *node = CreateParseNode(ITERATION_STMT, @1.first_line);
    node->kind = ITERATION_FOR;
    node->init = $3;
    node->cond = $4;
    node->step = NULL;
    AddChild(node, $6);
    $$ = (ParseNode *)node;
  }
  | FOR '(' expression_statement expression_statement expression ')' statement {
    ITERATION_STMT *node = CreateParseNode(ITERATION_STMT, @1.first_line);
    node->kind = ITERATION_FOR;
    node->init = $3;
    node->cond = $4;
    node->step = $5;
    AddChild(node, $7);
    $$ = (ParseNode *)node;
  }
  | FOR '(' declaration expression_statement expression ')' statement {
    ITERATION_STMT *node = CreateParseNode(ITERATION_STMT, @1.first_line);
    node->kind = ITERATION_FOR;
    node->init = $3;
    node->cond = $4;
    node->step = $5;
    AddChild(node, $7);
    $$ = (ParseNode *)node;
  }
  ;

jump_statement
  : GOTO identifier ';' {
    JUMP_STMT *node = CreateParseNode(JUMP_STMT, @1.first_line);
    node->kind = JUMP_GOTO;
    AddChild(node, $2);
    $$ = (ParseNode *)node;
  }
  | CONTINUE ';' {
    JUMP_STMT *node = CreateParseNode(JUMP_STMT, @1.first_line);
    node->kind = JUMP_CONTINUE;
    $$ = (ParseNode *)node;
  }
  | BREAK ';' {
    JUMP_STMT *node = CreateParseNode(JUMP_STMT, @1.first_line);
    node->kind = JUMP_BREAK;
    $$ = (ParseNode *)node;
  }
  | RETURN ';' {
    JUMP_STMT *node = CreateParseNode(JUMP_STMT, @1.first_line);
    node->kind = JUMP_RETURN;
    $$ = (ParseNode *)node;
  }
  | RETURN expression ';' {
    JUMP_STMT *node = CreateParseNode(JUMP_STMT, @1.first_line);
    node->kind = JUMP_RETURN;
    AddChild(node, $2);
    $$ = (ParseNode *)node;
  }
  ;

translation_unit
  : external_declaration {
    // ctx->parse_tree_top already have valid root node (T_UNIT)
    AddChild(ctx->parse_tree_top, $1);
    $$ = ctx->parse_tree_top;
  }
  | translation_unit external_declaration {
    AddChild($1, $2);
  }
  ;

external_declaration
  : function_definition { $$ = $1; }
  | declaration { $$ = $1; }
  ;

function_definition
  : declaration_specifiers declarator compound_statement {
    FUNCTION_DEF *node = CreateParseNode(FUNCTION_DEF, @1.first_line);
    node->specifiers = (LIST *)$1;
    node->declarator = $2;
    node->statements = (LIST *)$3;
    $$ = (ParseNode *)node;
  }
  ;

%%

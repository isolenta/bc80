%{
    /* based on ANSI C Yacc grammar: https://www.quut.com/c/ANSI-C-grammar-y.html */

    #define YY_DECL int rc_lex (yyscan_t yyscanner)

    #include <stdio.h>
    #include <libgen.h>

    #include "common/buffer.h"
    #include "common/mmgr.h"
    #include "rcc/parse_nodes.h"
    #include "rcc/rcc.h"

    #include "rc_parser.tab.h"
    #include "rc_lexer.yy.h"

    extern int rc_lex (yyscan_t yyscanner, rcc_ctx_t *ctx);

    void rc_error(yyscan_t scanner, rcc_ctx_t *ctx, const char *msg) {
      int adjusted_line = ctx->scanner_state.line_num + ctx->parser_state.last_pp_line - ctx->parser_state.last_actual_line;
      ctx->current_position.line = get_actual_position(ctx, ctx->scanner_state.line_num, &ctx->current_position.filename);
      ctx->current_position.pos = ctx->scanner_state.pos_num;
      yylloc.first_line = ctx->current_position.line;
      yylloc.first_column = ctx->scanner_state.pos_num;
      report_error(ctx, "%s", msg);
    }

    #define RULE_UNARY_EXPRESSION(target, _kind, op, _line) do {            \
      EXPRESSION *expr = (EXPRESSION *)CreateParseNode(EXPRESSION, _line);  \
      expr->kind = _kind;                                                   \
      AddChild((Node *)expr, op);                                           \
      target = (Node *)expr;                                                \
    } while(0)

    #define RULE_BINARY_EXPRESSION(target, _kind, op1, op2, _line) do {     \
      EXPRESSION *expr = (EXPRESSION *)CreateParseNode(EXPRESSION, _line);  \
      expr->kind = _kind;                                                   \
      AddChild((Node *)expr, op1);                                     \
      AddChild((Node *)expr, op2);                                     \
      target = (Node *)expr;                                           \
    } while(0)

    #define RULE_SPECIFIER(target, _kind, _line) do {       \
      SPECIFIER *node = CreateParseNode(SPECIFIER, _line);  \
      node->kind = _kind;                                   \
      node->name = NULL;                                    \
      target = (Node *)node;                           \
    } while(0)
%}

%lex-param {void* scanner}{rcc_ctx_t *ctx}
%parse-param {void* scanner}{rcc_ctx_t *ctx}

%union {
  char *strval;
  int ival;
  Node *node;
  ExpressionKind expr_kind;
}

%locations
%define parse.error custom
%define api.prefix {rc_}

%token  <ival> INT_LITERAL
%token  <strval> STRING_LITERAL ID
%token  STATIC_ASSERT SIZEOF END_OF_COMMENT WHITESPACE BACKSLASH
%token  PTR_OP INC_OP DEC_OP LEFT_OP RIGHT_OP LE_OP GE_OP EQ_OP NE_OP
%token  AND_OP OR_OP MUL_ASSIGN DIV_ASSIGN MOD_ASSIGN ADD_ASSIGN
%token  SUB_ASSIGN LEFT_ASSIGN RIGHT_ASSIGN AND_ASSIGN
%token  XOR_ASSIGN OR_ASSIGN ALIGN SECTION

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
%type <node> declaration static_assert_declaration external_declaration position_declaration
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
    $$ = (Node *)node;
  }
  | FALSE
  {
    LITERAL *node = CreateParseNode(LITERAL, @1.first_line);
    node->kind = LITBOOL;
    node->bval = false;
    $$ = (Node *)node;
  }
  | TRUE
  {
    LITERAL *node = CreateParseNode(LITERAL, @1.first_line);
    node->kind = LITBOOL;
    node->bval = true;
    $$ = (Node *)node;
  }
  ;

string
  : STRING_LITERAL
  {
    LITERAL *node = CreateParseNode(LITERAL, @1.first_line);
    node->kind = LITSTR;
    node->strval = $1;
    $$ = (Node *)node;
  };

identifier
  : ID
  {
    IDENTIFIER *node = CreateParseNode(IDENTIFIER, @1.first_line);
    node->name = $1;
    $$ = (Node *)node;
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
    Node *node = CreatePrimitiveParseNode(ARG_EXPR_LIST, @1.first_line);
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
    $$ = (Node *)node;
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
    Node *node = CreatePrimitiveParseNode(EXPR_LIST, @1.first_line);
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
    AddChild($1, $2);
    $$ = $1;
  }
  | static_assert_declaration { $$ = $1; }
  | position_declaration { $$ = $1; }
  ;

position_declaration
  : PP_LINE STRING_LITERAL INT_LITERAL {
    POSITION_DECL *node = CreateParseNode(POSITION_DECL, @1.first_line);
    node->file = $2;
    node->line = $3;
    ctx->parser_state.last_pp_line = $3;
    ctx->parser_state.last_actual_line = @1.first_line;
    ctx->current_position.filename = $2;
    $$ = (Node *)node;
  }
  ;

declaration_specifiers
  : storage_class_specifier declaration_specifiers {
    AddChild($1, $2);
    $$ = $1;
  }
  | storage_class_specifier {
    $$ = $1;
  }
  | type_specifier declaration_specifiers {
    AddChild($1, $2);
    $$ = $1;
  }
  | type_specifier {
    $$ = $1;
  }
  | function_specifier declaration_specifiers {
    AddChild($1, $2);
    $$ = $1;
  }
  | function_specifier {
    $$ = $1;
  }
  ;

init_declarator_list
  : init_declarator {
    Node *node = CreatePrimitiveParseNode(INIT_DECLARATOR_LIST, @1.first_line);
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
  | ALIGN '(' constant_expression ')' {
    RULE_SPECIFIER($$, SPEC_ALIGN, @1.first_line);
    SPECIFIER *spec = (SPECIFIER *)$$;
    spec->arg = $3;
  }
  | SECTION '(' string ')' {
    RULE_SPECIFIER($$, SPEC_SECTION, @1.first_line);
    SPECIFIER *spec = (SPECIFIER *)$$;
    spec->arg = $3;
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
    $$ = (Node *)node;
  }
  | STRUCT identifier {
    SPECIFIER *node = CreateParseNode(SPECIFIER, @1.first_line);
    node->kind = SPEC_STRUCT;
    node->name = (IDENTIFIER *)$2;
    $$ = (Node *)node;
  }
  ;

struct_declaration_list
  : struct_declaration {
    Node *list = CreatePrimitiveParseNode(STRUCT_DECL_LIST, @1.first_line);
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
  | static_assert_declaration { $$ = $1; }
  ;

struct_declarator_list
  : struct_declarator {
    Node *list = CreatePrimitiveParseNode(STRUCT_DECLARATOR_LIST, @1.first_line);
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
    Node *deepest_pointer = $1;

    while (1) {
      if (dynarray_length(deepest_pointer->childs->list) > 0) {
        Node *child = (Node *)dinitial(deepest_pointer->childs->list);
        if (child->type == T_POINTER) {
          deepest_pointer = child;
          continue;
        }
      }
      break;
    }

    AddChild(deepest_pointer, $2);
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
  | direct_declarator '[' ']' {
    RULE_BINARY_EXPRESSION($$, PFX_EXPR_ARRAY, $1, NULL, @1.first_line);
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
    Node *node = CreatePrimitiveParseNode(POINTER, @1.first_line);
    AddChild(node, $2);
    $$ = node;
  }
  | '*' {
    $$ = CreatePrimitiveParseNode(POINTER, @1.first_line);
  }
  ;

parameter_list
  : parameter_declaration {
    Node *list = CreatePrimitiveParseNode(PARAMETER_LIST, @1.first_line);
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
    Node *list = CreatePrimitiveParseNode(IDENTIFIER_LIST, @1.first_line);
    AddChild(list, $1);
    $$ = list;
  }
  | identifier_list ',' identifier {
    AddChild($1, $3);
  }
  ;

type_name
  : type_specifier pointer {
    Node *deepest_pointer = $2;

    while (1) {
      if (dynarray_length(deepest_pointer->childs->list) > 0) {
        Node *child = (Node *)dinitial(deepest_pointer->childs->list);
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
    Node *list = CreatePrimitiveParseNode(INITIALIZER_LIST, @1.first_line);
    Node *tuple = CreatePrimitiveParseNode(TUPLE, @1.first_line);
    AddChild(tuple, $1); // designator
    AddChild(tuple, $3); // initializer
    AddChild(list, tuple);
    $$ = list;
  }
  | initializer {
    Node *list = CreatePrimitiveParseNode(INITIALIZER_LIST, @1.first_line);
    AddChild(list, $1);
    $$ = list;
  }
  | initializer_list ',' designator_list '=' initializer {
    Node *tuple = CreatePrimitiveParseNode(TUPLE, @1.first_line);
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
    Node *node = CreatePrimitiveParseNode(DESIGNATOR_LIST, @1.first_line);
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
    $$ = (Node *)node;
  }
  | '.' identifier {
    DESIGNATOR *node = CreateParseNode(DESIGNATOR, @1.first_line);
    node->kind = DESIGNATOR_FIELD;
    AddChild(node, $2);
    $$ = (Node *)node;
  }
  ;

static_assert_declaration
  : STATIC_ASSERT '(' constant_expression ',' string ')' ';' {
    STATIC_ASSERT_DECL *node = CreateParseNode(STATIC_ASSERT_DECL, @1.first_line);
    node->expr = (EXPRESSION *)$3;
    node->message = (LITERAL *)$5;
    $$ = (Node *)node;
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
    $$ = (Node *)node;
  }
  | CASE constant_expression ':' statement {
    LABELED_STMT *node = CreateParseNode(LABELED_STMT, @1.first_line);
    node->kind = LABELED_CASE;
    node->name = $2;
    AddChild(node, $4);
    $$ = (Node *)node;
  }
  | DEFAULT ':' statement {
    LABELED_STMT *node = CreateParseNode(LABELED_STMT, @1.first_line);
    node->kind = LABELED_DEFAULT;
    node->name = NULL;
    AddChild(node, $3);
    $$ = (Node *)node;
  }
  ;

compound_statement
  : '{' '}' { $$ = (Node *)CreateParseNode(LIST, @1.first_line); }
  | '{'  block_item_list '}' { $$ = $2; }
  ;

block_item_list
  : block_item {
    Node *node = CreatePrimitiveParseNode(BLOCK_LIST, @1.first_line);
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
  : ';' { $$ = (Node *)NULL; }
  | expression ';' { $$ = $1; }
  ;

selection_statement
  : IF '(' expression ')' statement ELSE statement {
    SELECT_STMT *node = CreateParseNode(SELECT_STMT, @1.first_line);
    node->kind = SELECT_IF;
    node->expr = $3;
    AddChild(node, $5);
    AddChild(node, $7);
    $$ = (Node *)node;
  }
  | IF '(' expression ')' statement {
    SELECT_STMT *node = CreateParseNode(SELECT_STMT, @1.first_line);
    node->kind = SELECT_IF;
    node->expr = $3;
    AddChild(node, $5);
    $$ = (Node *)node;
  }
  | SWITCH '(' expression ')' statement {
    SELECT_STMT *node = CreateParseNode(SELECT_STMT, @1.first_line);
    node->kind = SELECT_SWITCH;
    node->expr = $3;
    AddChild(node, $5);
    $$ = (Node *)node;
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
    $$ = (Node *)node;
  }
  | DO statement WHILE '(' expression ')' ';' {
    ITERATION_STMT *node = CreateParseNode(ITERATION_STMT, @1.first_line);
    node->kind = ITERATION_DO;
    node->init = NULL;
    node->step = NULL;
    node->cond = $5;
    AddChild(node, $2);
    $$ = (Node *)node;
  }
  | FOR '(' expression_statement expression_statement ')' statement {
    ITERATION_STMT *node = CreateParseNode(ITERATION_STMT, @1.first_line);
    node->kind = ITERATION_FOR;
    node->init = $3;
    node->cond = $4;
    node->step = NULL;
    AddChild(node, $6);
    $$ = (Node *)node;
  }
  | FOR '(' declaration expression_statement ')' statement {
    ITERATION_STMT *node = CreateParseNode(ITERATION_STMT, @1.first_line);
    node->kind = ITERATION_FOR;
    node->init = $3;
    node->cond = $4;
    node->step = NULL;
    AddChild(node, $6);
    $$ = (Node *)node;
  }
  | FOR '(' expression_statement expression_statement expression ')' statement {
    ITERATION_STMT *node = CreateParseNode(ITERATION_STMT, @1.first_line);
    node->kind = ITERATION_FOR;
    node->init = $3;
    node->cond = $4;
    node->step = $5;
    AddChild(node, $7);
    $$ = (Node *)node;
  }
  | FOR '(' declaration expression_statement expression ')' statement {
    ITERATION_STMT *node = CreateParseNode(ITERATION_STMT, @1.first_line);
    node->kind = ITERATION_FOR;
    node->init = $3;
    node->cond = $4;
    node->step = $5;
    AddChild(node, $7);
    $$ = (Node *)node;
  }
  ;

jump_statement
  : GOTO identifier ';' {
    JUMP_STMT *node = CreateParseNode(JUMP_STMT, @1.first_line);
    node->kind = JUMP_GOTO;
    AddChild(node, $2);
    $$ = (Node *)node;
  }
  | CONTINUE ';' {
    JUMP_STMT *node = CreateParseNode(JUMP_STMT, @1.first_line);
    node->kind = JUMP_CONTINUE;
    $$ = (Node *)node;
  }
  | BREAK ';' {
    JUMP_STMT *node = CreateParseNode(JUMP_STMT, @1.first_line);
    node->kind = JUMP_BREAK;
    $$ = (Node *)node;
  }
  | RETURN ';' {
    JUMP_STMT *node = CreateParseNode(JUMP_STMT, @1.first_line);
    node->kind = JUMP_RETURN;
    $$ = (Node *)node;
  }
  | RETURN expression ';' {
    JUMP_STMT *node = CreateParseNode(JUMP_STMT, @1.first_line);
    node->kind = JUMP_RETURN;
    AddChild(node, $2);
    $$ = (Node *)node;
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
    $$ = (Node *)node;
  }
  ;

%%

static const char *yysymbol_name_human(yysymbol_kind_t yysymbol) {
  switch (yysymbol) {
    case YYSYMBOL_ID:
      return "identifier";
    case YYSYMBOL_INT_LITERAL:
      return "integer";
    case YYSYMBOL_STRING_LITERAL:
      return "string literal";
    case YYSYMBOL_ALIGN:
    case YYSYMBOL_SECTION:
    case YYSYMBOL_STATIC_ASSERT:
    case YYSYMBOL_SIZEOF:
    case YYSYMBOL_STATIC:
    case YYSYMBOL_INLINE:
    case YYSYMBOL_VOLATILE:
    case YYSYMBOL_INT8:
    case YYSYMBOL_UINT8:
    case YYSYMBOL_INT16:
    case YYSYMBOL_UINT16:
    case YYSYMBOL_VOID:
    case YYSYMBOL_STRUCT:
    case YYSYMBOL_FALSE:
    case YYSYMBOL_TRUE:
    case YYSYMBOL_BOOL:
    case YYSYMBOL_CASE:
    case YYSYMBOL_DEFAULT:
    case YYSYMBOL_IF:
    case YYSYMBOL_ELSE:
    case YYSYMBOL_SWITCH:
    case YYSYMBOL_WHILE:
    case YYSYMBOL_FOR:
    case YYSYMBOL_DO:
    case YYSYMBOL_GOTO:
    case YYSYMBOL_CONTINUE:
    case YYSYMBOL_BREAK:
    case YYSYMBOL_RETURN:
      return "keyword";
    default:
      return yysymbol_name(yysymbol);
  }
}

static int yyreport_syntax_error(const yypcontext_t *errctx, void *scanner, rcc_ctx_t *ctx) {
  buffer *errmsg = buffer_init();
  int res = 0, num_expected;
  enum { TOKENMAX = 5 };
  yysymbol_kind_t expected[TOKENMAX];

  num_expected = yypcontext_expected_tokens(errctx, expected, TOKENMAX);
  if (num_expected < 0) {
    res = num_expected;    // forward errors to yyparse
  } else {
    for (int i = 0; i < num_expected; ++i)
      buffer_append(errmsg, "%s %s",
               i == 0 ? "expected" : " or", yysymbol_name_human(expected[i]));
  }

  yysymbol_kind_t lookahead = yypcontext_token(errctx);
  if (lookahead != YYSYMBOL_YYEMPTY) {
    // adjust line number according #line directives
    ctx->current_position.line = get_actual_position(ctx, ctx->scanner_state.line_num, &ctx->current_position.filename);

    if (lookahead == YYSYMBOL_YYEOF) {
      buffer_append(errmsg, " until the end of file");
    } else {
      YYLTYPE *loc = yypcontext_location(errctx);

      char *err_token = get_token_at(ctx->scanner_state.source_ptr, loc->first_line, loc->first_column, loc->last_column - loc->first_column + 1);
      if (num_expected > 0)
        buffer_append(errmsg, " before '%s'", err_token);
      else
        buffer_append(errmsg, "unexpected %s '%s'", yysymbol_name_human(lookahead), err_token);
    }
  }

  char *errstr = buffer_dup(errmsg);
  buffer_free(errmsg);

  report_error(ctx, errstr);
  return res;
}

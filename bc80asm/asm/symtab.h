#pragma once

typedef struct parse_node parse_node;
typedef struct compile_ctx_t compile_ctx_t;
typedef struct hashmap hashmap;

extern hashmap *make_symtab(hashmap *defineopts);
extern parse_node *add_sym_variable_node(compile_ctx_t *ctx, const char *name, parse_node *value);
extern parse_node *add_sym_variable_integer(compile_ctx_t *ctx, const char *name, int ival);
extern parse_node *get_sym_variable(compile_ctx_t *ctx, const char *name, bool missing_ok);
extern parse_node *remove_sym_variable(compile_ctx_t *ctx, const char *name);

#include "common/mmgr.h"
#include "rcc/rcc.h"

// stage 2: parse source text and produce AST
void *do_parse(rcc_ctx_t *ctx, char *source) {
  return xmalloc(1);
}

char *dump_ast(void *ast) {
  return xstrdup("<AST>\n");
}

void ast_free(void *ast) {
  xfree(ast);
}

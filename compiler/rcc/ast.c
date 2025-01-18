#include "common/buffer.h"
#include "common/mmgr.h"
#include "rcc/parse_nodes.h"
#include "rcc/rcc.h"

void *do_ast(rcc_ctx_t *ctx, void *parse_tree) {
  return NULL;
}

char *dump_ast(void *ast) {
  char *result;

  buffer *dump_buf = buffer_init();
  buffer_append_char(dump_buf, '\n');
  result = buffer_dup(dump_buf);
  buffer_free(dump_buf);

  return result;
}

void ast_free(void *ast) {
}

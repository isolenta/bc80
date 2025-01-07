#include "common/mmgr.h"
#include "rcc/rcc.h"

// stage 3: produce assembler from AST
char *do_compile(rcc_ctx_t *ctx, void *ast) {
  return xstrdup("nop\n");
}

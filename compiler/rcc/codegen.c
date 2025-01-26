#include "common/mmgr.h"
#include "rcc/rcc.h"

// stage 4: produce assembler from semantics analysis artifacts
char *do_codegen(rcc_ctx_t *ctx) {
  return xstrdup("nop\n");
}

#include "common/buffer.h"
#include "rcc/rcc.h"

// stage 1: preprocessing
char *do_preproc(rcc_ctx_t *ctx, const char *source) {
  buffer *dest = buffer_init_ex("scan_dest");
  char *result;

  // ...

  buffer_append(dest, "/* preproc %s */\n", ctx->in_filename);

  result = buffer_dup(dest);
  buffer_free(dest);
  return result;
}

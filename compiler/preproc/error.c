#include <stdarg.h>

#include "buffer.h"
#include "preproc.h"
#include "error.h"

void report_error(preproc_ctx_t *ctx, char *fmt, ...) {
  if (ctx->error_cb) {
    va_list args;
    buffer *msgbuf = buffer_init();

    va_start(args, fmt);
    buffer_append_va(msgbuf, fmt, args);
    va_end(args);

    int err_cb_ret = ctx->error_cb(msgbuf->data);
    buffer_free(msgbuf);

    if (err_cb_ret != 0)
      longjmp(*ctx->error_jmp_env, 1);
  }
}

void report_warning(preproc_ctx_t *ctx, char *fmt, ...) {
  if (ctx->warning_cb) {
    va_list args;
    buffer *msgbuf = buffer_init();

    va_start(args, fmt);
    buffer_append_va(msgbuf, fmt, args);
    va_end(args);

    ctx->warning_cb(msgbuf->data);
    buffer_free(msgbuf);
  }
}

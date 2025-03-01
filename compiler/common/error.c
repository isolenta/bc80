#include <setjmp.h>
#include <stdarg.h>

#include "common/buffer.h"
#include "common/error.h"

struct error_context_t {
  error_callback_type error_cb;
  warning_callback_type warning_cb;
  info_callback_type info_cb;
  jmp_buf *error_env;
};

static struct error_context_t error_context = {0};

void set_error_context(error_callback_type error_cb, warning_callback_type warning_cb, info_callback_type info_cb, jmp_buf *error_env) {
  error_context.error_cb = error_cb;
  error_context.warning_cb = warning_cb;
  error_context.info_cb = info_cb;
  error_context.error_env = error_env;
}

void generic_report_error(int flags, const char *filename, int line, int pos, char *fmt, ...) {
  if (error_context.error_cb) {
    va_list args;
    buffer *msgbuf = buffer_init();

    va_start(args, fmt);
    buffer_append_va(msgbuf, fmt, args);
    va_end(args);

    int err_cb_ret = error_context.error_cb(flags, msgbuf->data, filename, line, pos);
    buffer_free(msgbuf);

    if (err_cb_ret != 0)
      longjmp(*(error_context.error_env), 1);
  }
}

void generic_report_warning(int flags, const char *filename, int line, int pos, char *fmt, ...) {
  if (error_context.warning_cb) {
    va_list args;
    buffer *msgbuf = buffer_init();

    va_start(args, fmt);
    buffer_append_va(msgbuf, fmt, args);
    va_end(args);

    error_context.warning_cb(flags, msgbuf->data, filename, line, pos);
    buffer_free(msgbuf);
  }
}

void generic_report_info(int flags, const char *filename, int line, int pos, char *fmt, ...) {
  if (error_context.info_cb) {
    va_list args;
    buffer *msgbuf = buffer_init();

    va_start(args, fmt);
    buffer_append_va(msgbuf, fmt, args);
    va_end(args);

    error_context.info_cb(flags, msgbuf->data, filename, line, pos);
    buffer_free(msgbuf);
  }
}

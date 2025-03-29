#include <setjmp.h>
#include <stdarg.h>
#include <libgen.h>

#include "bits/buffer.h"
#include "bits/error.h"

static jmp_buf *error_env;

void set_error_context(jmp_buf *error_env_) {
  error_env = error_env_;
}

void generic_report_error(int flags, const char *filename, int line, int pos, char *fmt, ...) {
  va_list args;
  buffer *msgbuf = buffer_init();

  va_start(args, fmt);
  buffer_append_va(msgbuf, fmt, args);
  va_end(args);

  if (flags & ERROR_OUT_LOC) {
    fprintf(stderr, "\x1b[97m%s", basename((char *)filename));
    if (flags & ERROR_OUT_LINE)
      fprintf(stderr, ":%d", line);
    if (flags & ERROR_OUT_POS)
      fprintf(stderr, ":%d", pos);
    fprintf(stderr, ":\x1b[0m ");
  }
  fprintf(stderr, "\x1b[91merror:\x1b[0m \x1b[97m%s\x1b[0m\n", msgbuf->data);

  buffer_free(msgbuf);

  longjmp(*error_env, 1);
}

void generic_report_warning(int flags, const char *filename, int line, int pos, char *fmt, ...) {
  va_list args;
  buffer *msgbuf = buffer_init();

  va_start(args, fmt);
  buffer_append_va(msgbuf, fmt, args);
  va_end(args);

  if (flags & ERROR_OUT_LOC) {
    fprintf(stderr, "\x1b[97m%s", basename((char *)filename));
    if (flags & ERROR_OUT_LINE)
      fprintf(stderr, ":%d", line);
    if (flags & ERROR_OUT_POS)
      fprintf(stderr, ":%d", pos);
    fprintf(stderr, ":\x1b[0m");
  }
  fprintf(stderr, " \x1b[95mwarning:\x1b[0m \x1b[97m%s\x1b[0m\n", msgbuf->data);

  buffer_free(msgbuf);
}

void report_info(char *fmt, ...) {
  va_list args;
  buffer *msgbuf = buffer_init();

  va_start(args, fmt);
  buffer_append_va(msgbuf, fmt, args);
  va_end(args);

  fprintf(stderr, "\x1b[97m%s\x1b[0m\n", msgbuf->data);
  buffer_free(msgbuf);
}

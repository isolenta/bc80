#pragma once

#include <setjmp.h>

#define ERROR_OUT_LOC  (1 << 0)
#define ERROR_OUT_LINE (1 << 1)
#define ERROR_OUT_POS  (1 << 2)

extern void set_error_context(jmp_buf *error_env);

extern void generic_report_error(int flags, const char *filename, int line, int pos, char *fmt, ...);
extern void generic_report_warning(int flags, const char *filename, int line, int pos, char *fmt, ...);
extern void report_info(char *fmt, ...);

#define report_error(ctx, fmt, ...) \
  do { \
    generic_report_error(ERROR_OUT_LOC | ERROR_OUT_LINE | ERROR_OUT_POS, \
      (ctx)->node->fn, (ctx)->node->line - 1, (ctx)->node->pos, \
      fmt, ## __VA_ARGS__); \
  } while (0)


#define report_warning(ctx, fmt, ...) \
  do { \
    generic_report_warning(ERROR_OUT_LOC | ERROR_OUT_LINE, \
      (ctx)->node->fn, (ctx)->node->line, 0, \
      fmt, ## __VA_ARGS__); \
  } while (0)

#define report_error_nopos(ctx, fmt, ...) \
  generic_report_error(ERROR_OUT_LOC | ERROR_OUT_LINE, (ctx)->node->fn, (ctx)->node->line - 1, 0, fmt, ## __VA_ARGS__);

#define report_error_noloc(fmt, ...) \
  generic_report_error(0, NULL, 0, 0, fmt, ## __VA_ARGS__);

#define report_warning_noloc(fmt, ...) \
  generic_report_warning(0, NULL, 0, 0, fmt, ## __VA_ARGS__);

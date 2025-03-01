#pragma once

#include <setjmp.h>

#define ERROR_OUT_LOC  (1 << 0)
#define ERROR_OUT_LINE (1 << 1)
#define ERROR_OUT_POS  (1 << 2)

// user-defined callback for error processing:
// message: short error message
// detail: verbose error message add-in (can be NULL)
// line: error line position in the source text
// return value:
//   0: continue processing (can be dangerous, compilation state can be corrupted after error)
//   non zero: stop processing and return this code as result of libasm_as()
typedef int (*error_callback_type) (int flags, const char *message, const char *filename, int line, int pos);

// the same as error_callback_type but for non-critical warnings
typedef void (*warning_callback_type) (int flags, const char *message, const char *filename, int line, int pos);

// the same as error_callback_type but for information messages
typedef void (*info_callback_type) (int flags, const char *message, const char *filename, int line, int pos);

extern void set_error_context(error_callback_type error_cb, warning_callback_type warning_cb, info_callback_type info_cb, jmp_buf *error_env);

extern void generic_report_error(int flags, const char *filename, int line, int pos, char *fmt, ...);
extern void generic_report_warning(int flags, const char *filename, int line, int pos, char *fmt, ...);
extern void generic_report_info(int flags, const char *filename, int line, int pos, char *fmt, ...);

#define report_error_noloc(fmt, ...) \
  generic_report_error( 0, NULL, 0, 0, fmt, ## __VA_ARGS__);

#define report_warning_noloc(ctx, fmt, ...) \
  generic_report_warning( 0, NULL, 0, 0, fmt, ## __VA_ARGS__);

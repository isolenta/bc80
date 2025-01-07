#pragma once

#include <setjmp.h>

// user-defined callback for error processing:
// message: short error message
// detail: verbose error message add-in (can be NULL)
// line: error line position in the source text
// return value:
//   0: continue processing (can be dangerous, compilation state can be corrupted after error)
//   non zero: stop processing and return this code as result of libasm_as()
typedef int (*error_callback_type) (const char *message, const char *filename, int line);

// the same as error_callback_type but for non-critical warnings
typedef void (*warning_callback_type) (const char *message, const char *filename, int line);

extern void set_error_context(error_callback_type error_cb, warning_callback_type warning_cb, jmp_buf *error_env);

extern void generic_report_error(const char *filename, int line, char *fmt, ...);
extern void generic_report_warning(const char *filename, int line, char *fmt, ...);

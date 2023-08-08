#pragma once

typedef struct preproc_ctx_t preproc_ctx_t;

typedef int (*error_callback_type) (const char *message);
typedef void (*warning_callback_type) (const char *message);

extern void report_error(preproc_ctx_t *ctx, char *fmt, ...);
extern void report_warning(preproc_ctx_t *ctx, char *fmt, ...);

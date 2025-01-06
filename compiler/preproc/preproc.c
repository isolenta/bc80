#include <stdio.h>
#include <stdbool.h>
#include <stdint.h>
#include <string.h>
#include <stdlib.h>
#include <stdarg.h>
#include <unistd.h>
#include <ctype.h>
#include <time.h>

#include "filesystem.h"
#include "mmgr.h"
#include "buffer.h"
#include "preproc.h"
#include "error.h"

char *read_file_content(preproc_ctx_t *ctx, char *filename) {
  char *source = NULL;
  size_t filesize, sret;
  FILE *fin = NULL;

  if (!fs_file_exists(filename))
    report_error(ctx, "file is not exist: %s\n", filename);

  filesize = fs_file_size(filename);
  fin = fopen(filename, "r");
  if (!fin)
    report_error(ctx, "unable to open file %s\n", filename);

  source = (char *)xmalloc(filesize + 2);

  sret = fread(source, filesize, 1, fin);
  if (sret != 1) {
    fclose(fin);
    report_error(ctx, "file reading error %s\n", filename);
  }

  source[filesize] = '\0';
  source[filesize + 1] = '\0';

  fclose(fin);

  return source;
}

char *do_preproc(preproc_ctx_t *ctx, const char *source, const char *filename) {
  buffer *dest = buffer_init_ex("scan_dest");
  char *result;

  // ...

  buffer_append(dest, "/* preproc %s */\n", filename);

  result = buffer_dup(dest);
  buffer_free(dest);
  return result;
}

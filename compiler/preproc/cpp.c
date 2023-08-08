#include <stdio.h>
#include <stdbool.h>
#include <stdint.h>
#include <string.h>
#include <stdlib.h>
#include <stdarg.h>
#include <unistd.h>
#include <ctype.h>
#include <setjmp.h>

#include "filesystem.h"
#include "mmgr.h"
#include "hashmap.h"
#include "buffer.h"
#include "preproc.h"
#include "error.h"

static jmp_buf preproc_env;

static int error_cb(const char *message) {
  fprintf(stderr, "\x1b[31m");
  fprintf(stderr, "Error: ");
  fprintf(stderr, "%s\x1b[0m\n", message);
  return 1;
}

static void warning_cb(const char *message) {
  fprintf(stderr, "\x1b[33m");
  fprintf(stderr, "Warning: ");
  fprintf(stderr, "%s\x1b[0m\n", message);
}

static void print_usage(char *cmd) {
  printf("Reduced C preprocessor\n\n"
         "usage: %s [options] <input file>\n\n"
         "options:\n"
         "  -h              this help\n"
         "  -Ipath          add directory to include path list (for preprocessor #include directive search)\n"
         "  -Dkey[=value]   define symbol for preprocessor\n",
         cmd
  );
}

int main(int argc, char **argv) {
  int optflag;
  int retval = 0;
  char *infile = NULL;
  char *source = NULL;
  preproc_ctx_t context;

  mmgr_init();

  context.unique_counter = 0;
  context.error_cb = error_cb;
  context.warning_cb = warning_cb;
  context.error_jmp_env = &preproc_env;
  context.includeopts = NULL;
  context.processed_files = hashmap_create(128, "processed_files");;
  context.constants = hashmap_create(128, "constants");
  context.macroses = hashmap_create(128, "macroses");

  opterr = 0;

  while ((optflag = getopt(argc, argv, "vhD:I:o:t:")) != -1) {
    switch (optflag) {
      case 'D': {
        dynarray *kvparts = split_string_sep(optarg, '=', true);
        hashmap_search(context.constants, dinitial(kvparts), HASHMAP_INSERT,
          (dynarray_length(kvparts) == 1) ? xstrdup("") : xstrdup(dsecond(kvparts)));
        break;
      }

      case 'I':
        context.includeopts = dynarray_append_ptr(context.includeopts, xstrdup(optarg));
        break;

      case 'h':
      default:
        print_usage(argv[0]);
        return 0;
        break;
    }
  }

  if (argc - optind >= 1)
    infile = xstrdup(argv[optind]);

  if (!infile) {
    print_usage(argv[0]);
    return 1;
  }

  retval = setjmp(preproc_env);
  if (retval != 0)
    // returning from longjmp (error handler in preprocessor)
    goto out;

  source = read_file_content(&context, infile);
  char *result = scan(&context, source, fs_abs_path(infile, NULL));
  if (result && *result)
    puts(result);
  xfree(result);

out:
  if (source)
    xfree(source);

  xfree(infile);

  hashmap_free(context.macroses);
  hashmap_free(context.constants);
  dynarray_free_deep(context.includeopts);

  mmgr_finish(getenv("MEMSTAT") != NULL);

  return retval;
}
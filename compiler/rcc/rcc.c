#include <getopt.h>
#include <setjmp.h>
#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>

#include "asm/libasm.h"
#include "common/buffer.h"
#include "common/dynarray.h"
#include "common/error.h"
#include "common/filesystem.h"
#include "common/hashmap.h"
#include "common/mmgr.h"
#include "rcc/rcc.h"

static jmp_buf rcc_env;

static int error_cb(const char *message, const char *filename, int line) {
  fprintf(stderr, "\x1b[31m");
  if (line > 0)
    fprintf(stderr, "Error in %s:%d: ", filename, line);
  else
    fprintf(stderr, "Error: ");
  fprintf(stderr, "%s\x1b[0m\n", message);
  return 1;
}

static void warning_cb(const char *message, const char *filename, int line) {
  fprintf(stderr, "\x1b[33m");
  if (line > 0)
    fprintf(stderr, "Warning in %s:%d: ", filename, line);
  else
    fprintf(stderr, "Warning: ");
  fprintf(stderr, "%s\x1b[0m\n", message);
}

static void print_usage(char *cmd) {
  printf("Reduced C compiler\n\n"
         "usage: %s [options] <input file>\n\n"
         "options:\n"
         "  -h                this help\n"
         "  -Ipath            add directory to include search path list\n"
         "  -Dkey[=value]     define symbol for preprocessor\n"
         "  -e                produce preprocessor output\n"
         "  -a                produce AST output\n"
         "  -s                produce assembler output\n"
         "  --pp-filename <filename>  if preprocessor output produced, use specified filename for result\n"
         "  --ast-filename <filename>  if AST output produced, use specified filename for result\n"
         "  --asm-filename <filename>  if assembler output produced, use specified filename for result\n"
         "  --obj-filename <filename>  use specified filename for object file produced\n"
         ,
         cmd
  );
}

int main(int argc, char **argv) {
  int optflag;
  int retval = 0;
  char *infile = NULL;
  char *source = NULL;
  char *pp_outfile = NULL;
  char *ast_outfile = NULL;
  char *asm_outfile = NULL;
  char *obj_outfile = NULL;
  bool out_pp = false;
  bool out_ast = false;
  bool out_asm = false;
  rcc_ctx_t context;

  mmgr_init();
  retval = setjmp(rcc_env);
  if (retval != 0) {
    // returning from longjmp (error handler)
    goto out;
  }
  set_error_context(error_cb, warning_cb, &rcc_env);

  memset(&context, 0, sizeof(context));

  context.constants = hashmap_create(128, "constants");
  context.pp_files = hashmap_create(128, "preprocessed files");

  opterr = 0;

  const struct option long_options[] = {
    {"pp-filename",          required_argument, NULL, 1},
    {"ast-filename",         required_argument, NULL, 2},
    {"asm-filename",         required_argument, NULL, 3},
    {"obj-filename",         required_argument, NULL, 4},
    {0, 0, 0, 0}
  };

  while ((optflag = getopt_long(argc, argv, "hD:I:eas", long_options, NULL)) != -1) {
    switch (optflag) {
      case 0:
        break;

      case 'D': {
        dynarray *kvparts = split_string_sep(optarg, '=', true);
        char *key = dinitial(kvparts);

        if (!is_identifier(key))
          generic_report_error("", 0, "invalid constant name: %s", key);

        if (is_keyword(key))
          generic_report_error("", 0, "can't redefine keyword: %s", key);

        char *value = (dynarray_length(kvparts) == 1) ? "" : dsecond(kvparts);

        hashmap_search(context.constants, key, HASHMAP_INSERT, xstrdup(value));
        break;
      }

      case 'I':
        context.includeopts = dynarray_append_ptr(context.includeopts, xstrdup(optarg));
        break;

      case 'e':
        out_pp = true;
        break;

      case 'a':
        out_ast = true;
        break;

      case 's':
        out_asm = true;
        break;

      case 1:
        pp_outfile = xstrdup(optarg);
        break;

      case 2:
        ast_outfile = xstrdup(optarg);
        break;

      case 3:
        asm_outfile = xstrdup(optarg);
        break;

      case 4:
        obj_outfile = xstrdup(optarg);
        break;

      case ':':
        fprintf(stderr, "missing option argument\n");
        break;

      case '?':
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

  if (out_pp && pp_outfile == NULL)
    pp_outfile = fs_replace_suffix(infile, "pp");
  if (out_ast && ast_outfile == NULL)
    ast_outfile = fs_replace_suffix(infile, "ast");
  if (out_asm && asm_outfile == NULL)
    asm_outfile = fs_replace_suffix(infile, "asm");
  if (obj_outfile == NULL)
    obj_outfile = fs_replace_suffix(infile, "obj");

  context.in_filename = fs_abs_path(infile, NULL);

  source = read_file(infile);

  // stage 1: preprocessing
  char *preproc_output = do_preproc(&context, source);
  if (out_pp && preproc_output) {
    write_file(preproc_output, strlen(preproc_output), pp_outfile);
  }

  // stage 2: parse source text and produce AST
  void *ast_output = do_parse(&context, preproc_output);
  if (out_ast && ast_output) {
    char *ast_str = dump_ast(ast_output);
    write_file(ast_str, strlen(ast_str), ast_outfile);
    xfree(ast_str);
  }

  // stage 3: produce assembler from AST
  char *asm_output = do_compile(&context, ast_output);
  if (out_asm && asm_output) {
    write_file(asm_output, strlen(asm_output), asm_outfile);
  }

  // stage 4: produce binary (ELF object) from assembler
  struct libasm_as_desc_t as_desc;
  char *obj_dest;
  uint32_t obj_size;

  memset(&as_desc, 0, sizeof(as_desc));
  as_desc.source = asm_output;
  as_desc.filename = out_asm ? asm_outfile : "<assembler>";
  as_desc.target = ASM_TARGET_RAW;//ASM_TARGET_ELF;
  as_desc.dest = &obj_dest;
  as_desc.dest_size = &obj_size;

  libasm_as(&as_desc);
  write_file(obj_dest, obj_size, obj_outfile);

  xfree(preproc_output);
  ast_free(ast_output);
  xfree(asm_output);

out:
  if (source)
    xfree(source);

  xfree(infile);

  hashmap_entry *entry = NULL;
  hashmap_scan *scan;

  scan = hashmap_scan_init(context.constants);
  while ((entry = hashmap_scan_next(scan)) != NULL) {
    char *parg = (char *)entry->value;
    xfree(parg);
  }

  hashmap_free(context.constants);
  dynarray_free_deep(context.includeopts);

  mmgr_finish(getenv("MEMSTAT") != NULL);

  return retval;
}
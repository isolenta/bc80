#include <getopt.h>
#include <libgen.h>
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
#include "rccompiler/rcc.h"

static jmp_buf rcc_env;

static int error_cb(int flags, const char *message, const char *filename, int line, int pos) {
  if (flags & ERROR_OUT_LOC) {
    fprintf(stderr, "\x1b[97m%s", basename((char *)filename));
    if (flags & ERROR_OUT_LINE)
      fprintf(stderr, ":%d", line);
    if (flags & ERROR_OUT_POS)
      fprintf(stderr, ":%d", pos);
    fprintf(stderr, ":\x1b[0m");
  }
  fprintf(stderr, " \x1b[91merror:\x1b[0m \x1b[97m%s\x1b[0m\n", message);
  return 1;
}

static void warning_cb(int flags, const char *message, const char *filename, int line, int pos) {
  if (flags & ERROR_OUT_LOC) {
    fprintf(stderr, "\x1b[97m%s", basename((char *)filename));
    if (flags & ERROR_OUT_LINE)
      fprintf(stderr, ":%d", line);
    if (flags & ERROR_OUT_POS)
      fprintf(stderr, ":%d", pos);
    fprintf(stderr, ":\x1b[0m");
  }
  fprintf(stderr, " \x1b[95mwarning:\x1b[0m \x1b[97m%s\x1b[0m\n", message);
}

static void print_usage(char *cmd) {
  printf("Reduced C compiler\n\n"
         "usage: %s [options] <input file>\n\n"
         "options:\n"
         "  -h                this help\n"
         "  -Ipath            add directory to include search path list\n"
         "  -Dkey[=value]     define symbol for preprocessor\n"
         "  -E                only preprocessor stage\n"
         "  -P                only preprocessor and parse tree stages\n"
         "  -A                only preprocessor, parse tree, semantics stages\n"
         "  -e                produce preprocessor output\n"
         "  -p                produce parse tree output\n"
         "  -a                produce semantics output\n"
         "  -s                produce assembler output\n"
         "  --pp-filename <filename>   if preprocessor output produced, use specified filename for result\n"
         "  --pt-filename <filename>   if parse tree output produced, use specified filename for result\n"
         "  --sem-filename <filename>  if semantics output produced, use specified filename for result\n"
         "  --asm-filename <filename>  if assembler output produced, use specified filename for result\n"
         "  --obj-filename <filename>  use specified filename for object file produced\n"
         ,
         cmd
  );
}

enum {
  STAGE_NONE = 0,
  STAGE_PREPROC,
  STAGE_PARSETREE,
  STAGE_SEMANTICS,
  STAGE_CODEGEN,
  STAGE_ASM,
  STAGE_LAST,
} rcc_processing_stage;

int main(int argc, char **argv) {
  int optflag;
  int retval = 0;
  char *infile = NULL;
  char *source = NULL;
  char *pp_outfile = NULL;
  char *pt_outfile = NULL;
  char *sem_outfile = NULL;
  char *asm_outfile = NULL;
  char *obj_outfile = NULL;
  bool out_pp = false;
  bool out_pt = false;
  bool out_sem = false;
  bool out_asm = false;
  int last_stage = STAGE_LAST;
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
    {"pt-filename",          required_argument, NULL, 2},
    {"sem-filename",         required_argument, NULL, 3},
    {"asm-filename",         required_argument, NULL, 4},
    {"obj-filename",         required_argument, NULL, 5},
    {0, 0, 0, 0}
  };

  while ((optflag = getopt_long(argc, argv, "hD:I:epasEPA", long_options, NULL)) != -1) {
    switch (optflag) {
      case 0:
        break;

      case 'D': {
        dynarray *kvparts = split_string_sep(optarg, '=', true);
        char *key = dinitial(kvparts);

        if (!is_identifier(key))
          report_error_noloc("invalid constant name: %s", key);

        if (is_keyword(key))
          report_error_noloc("can't redefine keyword: %s", key);

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

      case 'p':
        out_pt = true;
        break;

      case 'a':
        out_sem = true;
        break;

      case 's':
        out_asm = true;
        break;

      case 1:
        pp_outfile = xstrdup(optarg);
        break;

      case 2:
        pt_outfile = xstrdup(optarg);
        break;

      case 3:
        sem_outfile = xstrdup(optarg);
        break;

      case 4:
        asm_outfile = xstrdup(optarg);
        break;

      case 5:
        obj_outfile = xstrdup(optarg);
        break;

      case ':':
        fprintf(stderr, "missing option argument\n");
        break;

      case 'E':
        last_stage = STAGE_PREPROC;
        break;

      case 'P':
        last_stage = STAGE_PARSETREE;
        break;

      case 'A':
        last_stage = STAGE_SEMANTICS;
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
  if (out_pt && pt_outfile == NULL)
    pt_outfile = fs_replace_suffix(infile, "pt");
  if (out_sem && sem_outfile == NULL)
    sem_outfile = fs_replace_suffix(infile, "sem");
  if (out_asm && asm_outfile == NULL)
    asm_outfile = fs_replace_suffix(infile, "asm");
  if (obj_outfile == NULL)
    obj_outfile = fs_replace_suffix(infile, "obj");

  context.in_filename = fs_abs_path(infile, NULL);

  source = read_file(infile);

  char *preproc_output = NULL;
  void *pt_output = NULL;
  void *sem_output = NULL;
  char *asm_output = NULL;

  if (last_stage == STAGE_NONE)
    goto out;

  // stage 1: preprocessing
  preproc_output = do_preproc(&context, source);
  if (out_pp && preproc_output) {
    write_file(preproc_output, strlen(preproc_output), pp_outfile);
  }

  context.pp_output_str = preproc_output;

  if (last_stage == STAGE_PREPROC)
    goto out;

  // stage 2: parse source text and produce parse tree
  pt_output = do_parse(&context, preproc_output);
  if (out_pt && pt_output) {
    char *pt_str = dump_parse_tree(pt_output);
    write_file(pt_str, strlen(pt_str), pt_outfile);
    xfree(pt_str);
  }

  if (last_stage == STAGE_PARSETREE)
    goto out;

  // stage 3: semantics analysis based on parse tree
  do_semantics(&context, pt_output);
  if (out_sem) {
    char *sem_str = dump_semantics(&context);
    write_file(sem_str, strlen(sem_str), sem_outfile);
    xfree(sem_str);
  }

  if (last_stage == STAGE_SEMANTICS)
    goto out;

  // stage 4: produce assembler from semantics analysis artifacts
  asm_output = do_codegen(&context);
  if (out_asm && asm_output) {
    write_file(asm_output, strlen(asm_output), asm_outfile);
  }

  if (last_stage == STAGE_CODEGEN)
    goto out;

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

  if (last_stage == STAGE_ASM)
    goto out;

out:
  if (preproc_output)
    xfree(preproc_output);

  if (pt_output)
    parse_tree_free(pt_output);

  if (sem_output)
    semantics_free(&context);

  if (asm_output)
    xfree(asm_output);

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
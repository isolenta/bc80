/*
  Assembler for Z80 CPU target
*/

#include <stdio.h>
#include <string.h>
#include <unistd.h>
#include <sys/stat.h>
#include <sys/errno.h>

#include "common.h"
#include "dynarray.h"
#include "hashmap.h"

#include "parse.h"
#include "compile.h"
#include "filesystem.h"

static void print_usage(char *cmd) {
  printf("Assembler for Z80 CPU target.\n\n"
         "usage: %s [options] <input file>\n\n"
         "options:\n"
         "  -h              this help\n"
         "  -o filename     name of target file (will use input file name if omitted)\n"
         "  -Ipath          add directory to include path list (for preprocessor #include directive search)\n"
         "  -Dkey[=value]   define symbol for preprocessor\n"
         "  -t <target>     set output file target type. Can be one of:\n"
         "     raw (default)       raw binary rendered from absolute offset specified by ORG directive.\n"
         "                         Source file can contain only single section.\n"
         "     object              ELF object file. Source file can define multiple sections.\n",
         cmd
  );
}

int main(int argc, char **argv) {
  int optflag;
  dynarray *includeopts = NULL;
  hashmap *defineopts = NULL;
  dynarray_cell *dc = NULL;
  char *infile = NULL;
  char *outfile = NULL;
  int target = ASM_TARGET_RAW;
  int ret;
  size_t sz;
  FILE *fin = NULL;
  char *source = NULL;

  defineopts = hashmap_create(128);

  opterr = 0;

  while ((optflag = getopt(argc, argv, "vhD:I:o:t:")) != -1) {
    switch (optflag) {
      case 'D': {
        dynarray *kvparts = split_string_sep(optarg, '=', true);
        hashmap_search(defineopts, dinitial(kvparts), HASHMAP_INSERT,
          (dynarray_length(kvparts) == 1) ? strdup("") : strdup(dsecond(kvparts)));
        break;
      }

      case 'I':
        includeopts = dynarray_append_ptr(includeopts, strdup(optarg));
        break;

      case 'o':
        outfile = strdup(optarg);
        break;

      case 't':
        if (strcasecmp(optarg, "raw") == 0)
          target = ASM_TARGET_RAW;
        else if (strcasecmp(optarg, "object") == 0)
          target = ASM_TARGET_ELF;
        else {
          fprintf(stderr, "target must be one of 'raw', 'object'\n");
          return 1;
        }
        break;

      case 'h':
      default:
        print_usage(argv[0]);
        return 0;
        break;
    }
  }

  if (argc - optind >= 1)
    infile = strdup(argv[optind]);

  if (infile == NULL) {
    fprintf(stderr, "error: no input file specified\n\n");
    print_usage(argv[0]);
    return 1;
  }

  if (outfile == NULL) {
    // evaluate output filename from input one
    outfile = fs_replace_suffix(infile, (target == ASM_TARGET_RAW) ? "bin" : "obj");
  }

  // actual work below
  if (!fs_file_exists(infile)) {
    fprintf(stderr, "unable to open %s\n", infile);
    goto out;
  }

  sz = fs_file_size(infile);
  fin = fopen(infile, "r");
  if (!fin) {
    perror(infile);
    goto out;
  }

  source = (char *)malloc(sz);

  sz = fread(source, sz, 1, fin);
  if (sz != 1) {
    perror(infile);
    goto out;
  }

  dynarray *parse = NULL;
  parse_string(&parse, includeopts, source, infile);
  compile(parse, defineopts, includeopts, outfile, target);

out:
  if (fin)
    fclose(fin);

  if (source)
    free(source);

  free(outfile);
  free(infile);
  hashmap_free(defineopts);
  dynarray_free_deep(includeopts);

  return 0;
}

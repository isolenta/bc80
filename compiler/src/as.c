/*
  Assembler for z80 target
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
  printf("Assembler for Z80 CPU target. Produces raw binary file.\n\n"
         "usage: %s [options] <input file>\n\n"
         "options:\n"
         "  -v              be verbose\n"
         "  -h              this help\n"
         "  -o filename     name of target file (will use input file name if omitted)\n"
         "  -Ipath          add directory to include path list (for preprocessor #include directive search)\n"
         "  -Dkey[=value]   define symbol for preprocessor\n",
         cmd
  );
}

static void do_work(char *source,
                    hashmap *defineopts,
                    dynarray *includeopts,
                    char *infile,
                    char *outfile,
                    bool verbose) {
  dynarray *parse;

  dynarray_cell *dc = NULL;

  parse = NULL;

  parse_string(&parse, includeopts, source, infile);

  if (verbose) {
    printf("PARSE TREE:\n\n");
    parse_print(parse);
  }

  compile(parse, defineopts, includeopts, outfile);
}

int main(int argc, char **argv) {
  int optflag;
  dynarray *includeopts = NULL;
  hashmap *defineopts = NULL;
  dynarray_cell *dc = NULL;
  bool verbose = false;
  char *infile = NULL;
  char *outfile = NULL;
  int ret;
  size_t sz;
  FILE *fin = NULL;
  char *source = NULL;

  defineopts = hashmap_create(128);

  opterr = 0;

  while ((optflag = getopt(argc, argv, "vhD:I:o:")) != -1) {
    switch (optflag) {
      case 'v':
        verbose = true;
        break;

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
    outfile = fs_replace_suffix(infile, "obj");
  }

  if (verbose) {
    hashmap_scan *scan = NULL;
    hashmap_entry *defentry = NULL;

    printf("source: %s\n", infile);
    printf("destination: %s\n", outfile);

    printf("defines:\n");
    scan = hashmap_scan_init(defineopts);
    while ((defentry = hashmap_scan_next(scan)) != NULL)
      printf(" %s => %s\n", defentry->key, (char *)defentry->value);

    printf("include search paths:\n");
    foreach(dc, includeopts)
      printf(" %s\n", (char *)dfirst(dc));
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

  do_work(source, defineopts, includeopts, infile, outfile, verbose);

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

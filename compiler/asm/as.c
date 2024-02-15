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
#include "libasm.h"

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

static int error_cb(const char *message, const char *filename, int line) {
  fprintf(stderr, "\x1b[31m");
  fprintf(stderr, "Error in %s:%d: ", filename, line);
  fprintf(stderr, "%s\x1b[0m\n", message);
  return 1;
}

static void warning_cb(const char *message, const char *filename, int line) {
  fprintf(stderr, "\x1b[33m");
  fprintf(stderr, "Warning in %s:%d: ", filename, line);
  fprintf(stderr, "%s\x1b[0m\n", message);
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
  size_t filesize, sret;
  FILE *fin = NULL;
  FILE *fout = NULL;
  char *source = NULL;
  char *destination = NULL;
  uint32_t dest_size = 0;
  struct libasm_as_desc_t desc;

  mmgr_init();

  defineopts = hashmap_create(128, "defineopts");

  opterr = 0;

  while ((optflag = getopt(argc, argv, "vhD:I:o:t:")) != -1) {
    switch (optflag) {
      case 'D': {
        dynarray *kvparts = split_string_sep(optarg, '=', true);
        hashmap_search(defineopts, dinitial(kvparts), HASHMAP_INSERT,
          (dynarray_length(kvparts) == 1) ? xstrdup("") : xstrdup(dsecond(kvparts)));
        break;
      }

      case 'I':
        includeopts = dynarray_append_ptr(includeopts, xstrdup(optarg));
        break;

      case 'o':
        outfile = xstrdup(optarg);
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
    infile = xstrdup(argv[optind]);

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

  filesize = fs_file_size(infile);
  fin = fopen(infile, "r");
  if (!fin) {
    perror(infile);
    goto out;
  }

  source = (char *)xmalloc(filesize + 1);

  sret = fread(source, filesize, 1, fin);
  if (sret != 1) {
    perror(infile);
    goto out;
  }

  source[filesize] = '\0';

  memset(&desc, 0, sizeof(desc));
  desc.source = source;
  desc.filename = infile;
  desc.includeopts = includeopts;
  desc.defineopts = defineopts;
  desc.target = target;
  desc.error_cb = error_cb;
  desc.warning_cb = warning_cb;
  desc.dest = &destination;
  desc.dest_size = &dest_size;

  ret = libasm_as(&desc);

  fout = fopen(outfile, "w");
  if (!fout) {
    perror(outfile);
    goto out;
  }

  if (dest_size > 0) {
    sret = fwrite(destination, dest_size, 1, fout);
    if (sret != 1) {
      perror(outfile);
    }
    printf("%u bytes written to %s\n", dest_size, outfile);
  }

out:
  if (fin)
    fclose(fin);

  if (fout)
    fclose(fin);

  if (source)
    xfree(source);

  if (destination)
    xfree(destination);

  xfree(outfile);
  xfree(infile);
  hashmap_free(defineopts);
  dynarray_free_deep(includeopts);

  mmgr_finish(getenv("MEMSTAT") != NULL);

  return ret;
}

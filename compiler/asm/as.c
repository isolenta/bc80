/*
  Assembler for Z80 CPU target
*/

#include <getopt.h>
#include <stdio.h>
#include <string.h>
#include <libgen.h>

#include "asm/libasm.h"
#include "asm/snafmt.h"
#include "common/dynarray.h"
#include "common/error.h"
#include "common/filesystem.h"
#include "common/hashmap.h"
#include "common/mmgr.h"

static void print_usage(char *cmd) {
  printf("Assembler for Z80 CPU target.\n\n"
         "usage: %s [options] <input file>\n\n"
         "options:\n"
         "  -h              this help\n"
         "  -o filename     name of target file (will use input file name if omitted)\n"
         "  -Ipath          add directory to include path list (for preprocessor #include directive search)\n"
         "  -Dkey[=value]   define symbol for preprocessor\n"
         "  --profile[=all] enable profiling for blocks between global labels (or all labels if 'all' specified)\n"
         "  --profile-data  if profiling enabled, show information for data blocks (e.g. DB with labels)\n"
         "  -t target       set output file target type. Can be one of:\n"
         "     raw (default)       raw binary rendered from absolute offset specified by ORG directive.\n"
         "                         Source file can contain only single section.\n"
         "     object              ELF object file. Source file can define multiple sections.\n"
         "     sna                 RAM snapshot file (SNA, 48k uncompressed)\n"
         "\nFollowing options are valid only for sna target:\n"
         "  --sna-generic      don't initialize RAM in the shapshot (default: initialize for ZX Spectrum device)\n"
         "  --sna-pc value     initial value of PC (default: lowest section address)\n"
         "  --sna-ramtop addr  address for RAM top (zxspectrum; default 5D5Bh) or initial stack (generic; default 4000h)\n",
         cmd
  );
}

static int error_cb(int flags, const char *message, const char *filename, int line, int pos) {
  if (flags & ERROR_OUT_LOC) {
    fprintf(stderr, "\x1b[97m%s", basename((char *)filename));
    if (flags & ERROR_OUT_LINE)
      fprintf(stderr, ":%d", line);
    if (flags & ERROR_OUT_POS)
      fprintf(stderr, ":%d", pos);
    fprintf(stderr, ":\x1b[0m ");
  }
  fprintf(stderr, "\x1b[91merror:\x1b[0m \x1b[97m%s\x1b[0m\n", message);
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

static void info_cb(int flags, const char *message, const char *filename, int line, int pos) {
  fprintf(stderr, "\x1b[97m%s\x1b[0m\n", message);
}

static jmp_buf error_env;

enum {
  LONGOPT_SNA_GENERIC = 1,
  LONGOPT_SNA_PC,
  LONGOPT_SNA_RAMTOP,
  LONGOPT_PROFILE,
  LONGOPT_PROFILE_DATA,
};

int main(int argc, char **argv) {
  int optflag;
  dynarray *includeopts = NULL;
  hashmap *defineopts = NULL;
  dynarray_cell *dc = NULL;
  char *infile = NULL;
  char *outfile = NULL;
  int target = ASM_TARGET_RAW;
  int sna_generic = 0;
  int sna_pc_addr = -1;
  int sna_ramtop = -1;
  int profile_mode = 0;
  bool profile_data = false;
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

  const struct option long_options[] = {
    {"sna-generic",  no_argument,       &sna_generic, LONGOPT_SNA_GENERIC},
    {"sna-pc",       required_argument, 0,            LONGOPT_SNA_PC},
    {"sna-ramtop",   required_argument, 0,            LONGOPT_SNA_RAMTOP},
    {"profile",      optional_argument, 0,            LONGOPT_PROFILE},
    {"profile-data", no_argument,       0,            LONGOPT_PROFILE_DATA},
    {0, 0, 0, 0}
  };

  profile_mode = PROFILE_NONE;

  while ((optflag = getopt_long(argc, argv, "hD:I:o:t:", long_options, NULL)) != -1) {
    switch (optflag) {
      case 0:
        // no_argument option processed
        break;

      case LONGOPT_SNA_PC:
        if (!parse_any_integer(optarg, &sna_pc_addr)) {
          fprintf(stderr, "can't parse value for PC address: %s\n", optarg);
          return 1;
        }
        break;

      case LONGOPT_SNA_RAMTOP:
        if (!parse_any_integer(optarg, &sna_ramtop)) {
          fprintf(stderr, "can't parse value for ramtop address: %s\n", optarg);
          return 1;
        }
        break;

      case LONGOPT_PROFILE:
        if (optarg == NULL)
          profile_mode = PROFILE_GLOBALS;
        else if (strcmp(optarg, "all") == 0)
          profile_mode = PROFILE_ALL;
        else {
          fprintf(stderr, "invalid profile mode: %s\n", optarg);
          return 1;
        }
        break;

      case LONGOPT_PROFILE_DATA:
        profile_data = true;
        break;

      case 'D': {
        dynarray *kvparts = split_string_sep(optarg, '=', true);
        hashmap_set(defineopts, dinitial(kvparts),
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
        else if (strcasecmp(optarg, "sna") == 0)
          target = ASM_TARGET_SNA;
        else {
          fprintf(stderr, "target must be one of 'raw', 'object', 'sna'\n");
          return 1;
        }
        break;

      case ':':
        fprintf(stderr, "missing option argument\n");
        break;

      case 'h':
      case '?':
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
    outfile = fs_replace_suffix(infile,
      (target == ASM_TARGET_ELF) ? "obj" :
      (target == ASM_TARGET_SNA) ? "sna" :
      "bin");
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

  if (sna_ramtop == -1)
    sna_ramtop = (sna_generic != 0) ? SNA_DEFAULT_RAMTOP : ZX_DEFAULT_RAMTOP;

  memset(&desc, 0, sizeof(desc));
  desc.source = source;
  desc.filename = infile;
  desc.includeopts = includeopts;
  desc.defineopts = defineopts;
  desc.target = target;
  desc.sna_generic = (sna_generic != 0);
  desc.sna_pc_addr = sna_pc_addr;
  desc.sna_ramtop = sna_ramtop;
  desc.dest = &destination;
  desc.dest_size = &dest_size;
  desc.profile_mode = profile_mode;
  desc.profile_data = profile_data;

  ret = setjmp(error_env);
  if (ret != 0)
    // returning from longjmp (error handler)
    goto out;

  set_error_context(error_cb, warning_cb, info_cb, &error_env);

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

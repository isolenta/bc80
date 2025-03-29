#include <getopt.h>
#include <stdio.h>
#include <string.h>

#include "asm/bc80asm.h"
#include "asm/compile.h"
#include "asm/parse.h"
#include "asm/snafmt.h"
#include "bits/dynarray.h"
#include "bits/error.h"
#include "bits/filesystem.h"
#include "bits/hashmap.h"
#include "bits/mmgr.h"

static void print_usage(char *cmd)
{
  printf("Assembler for Z80 CPU\n\n"
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

static jmp_buf error_env;
dynarray *g_includeopts;

enum {
  LONGOPT_SNA_GENERIC = 1,
  LONGOPT_SNA_PC,
  LONGOPT_SNA_RAMTOP,
  LONGOPT_PROFILE,
  LONGOPT_PROFILE_DATA,
};

int main(int argc, char **argv)
{
  int optflag;
  char *outfile = NULL;
  size_t filesize, sret;
  FILE *fin = NULL;
  FILE *fout = NULL;
  char *infile = NULL;
  char *source = NULL;
  char *dest_buf = NULL;
  hashmap *defineopts;

  compile_opts opts = {0};
  g_includeopts = NULL;

  int ret = setjmp(error_env);
  if (ret != 0)
    // returning from longjmp (error handler)
    goto out;

  set_error_context(&error_env);

  mmgr_init();

  defineopts = hashmap_create(128, "defineopts");

  opterr = 0;

  const struct option long_options[] = {
    {"sna-generic",  no_argument,       0,            LONGOPT_SNA_GENERIC},
    {"sna-pc",       required_argument, 0,            LONGOPT_SNA_PC},
    {"sna-ramtop",   required_argument, 0,            LONGOPT_SNA_RAMTOP},
    {"profile",      optional_argument, 0,            LONGOPT_PROFILE},
    {"profile-data", no_argument,       0,            LONGOPT_PROFILE_DATA},
    {0, 0, 0, 0}
  };

  while ((optflag = getopt_long(argc, argv, "hD:I:o:t:", long_options, NULL)) != -1) {
    switch (optflag) {
      case 0:
        // no_argument option processed
        break;

      case LONGOPT_SNA_GENERIC:
        opts.sna_generic = true;
        break;

      case LONGOPT_SNA_PC:
        if (!parse_any_integer(optarg, &opts.sna_pc_addr))
          report_error_noloc("can't parse value for PC address: %s", optarg);
        break;

      case LONGOPT_SNA_RAMTOP:
        if (!parse_any_integer(optarg, &opts.sna_ramtop))
          report_error_noloc("can't parse value for ramtop address: %s", optarg);
        break;

      case LONGOPT_PROFILE:
        if (optarg == NULL)
          opts.profile_mode = PROFILE_GLOBALS;
        else if (strcmp(optarg, "all") == 0)
          opts.profile_mode = PROFILE_ALL;
        else
          report_error_noloc("invalid profile mode: %s", optarg);
        break;

      case LONGOPT_PROFILE_DATA:
        opts.profile_data = true;
        break;

      case 'D': {
        dynarray *kvparts = split_string_sep(optarg, '=', true);
        hashmap_set(defineopts, dinitial(kvparts),
          (dynarray_length(kvparts) == 1) ? xstrdup("") : xstrdup(dsecond(kvparts)));
        break;
      }

      case 'I':
        g_includeopts = dynarray_append_ptr(g_includeopts, xstrdup(optarg));
        break;

      case 'o':
        outfile = xstrdup(optarg);
        break;

      case 't':
        if (strcasecmp(optarg, "raw") == 0)
          opts.target = ASM_TARGET_RAW;
        else if (strcasecmp(optarg, "object") == 0)
          opts.target = ASM_TARGET_ELF;
        else if (strcasecmp(optarg, "sna") == 0)
          opts.target = ASM_TARGET_SNA;
        else
          report_error_noloc("target must be one of 'raw', 'object', 'sna'");
        break;

      case ':':
        report_error_noloc("missing option argument");
        break;

      case 'h':
      case '?':
      default:
        print_usage(argv[0]);
        return 0;
        break;
    }
  }

  if (opts.sna_ramtop == -1)
    opts.sna_ramtop = opts.sna_generic ? SNA_DEFAULT_RAMTOP : ZX_DEFAULT_RAMTOP;

  if (argc - optind >= 1)
    infile = xstrdup(argv[optind]);

  if (infile == NULL)
    report_error_noloc("error: no input file specified\n");

  if (outfile == NULL) {
    // evaluate output filename from input one
    outfile = fs_replace_suffix(infile,
      (opts.target == ASM_TARGET_ELF) ? "obj" :
      (opts.target == ASM_TARGET_SNA) ? "sna" :
      "bin");
  }

  // actual work below
  if (!fs_file_exists(infile))
    report_error_noloc("%s: file not found", infile);

  filesize = fs_file_size(infile);
  fin = fopen(infile, "r");
  if (!fin)
    report_error_noloc("%s: can't open for reading", infile);

  source = (char *)xmalloc(filesize + 1);

  sret = fread(source, filesize, 1, fin);
  if (sret != 1)
    report_error_noloc("%s: can't read", infile);

  source[filesize] = '\0';

  dynarray *statements = NULL;

  ret = parse_source(infile, source, &statements);
  if (ret != 0)
    report_error_noloc("parse %s error", infile);

  uint32_t dest_size = compile(opts, defineopts, statements, &dest_buf);

  fout = fopen(outfile, "w");
  if (!fout)
    report_error_noloc("%s: can't open for writing", outfile);

  if (dest_size > 0) {
    sret = fwrite(dest_buf, dest_size, 1, fout);
    if (sret != 1)
      report_error_noloc("%s: can't write", outfile);

    report_info("%u bytes written to %s", dest_size, outfile);
  }

out:
  if (fin)
    fclose(fin);

  if (fout)
    fclose(fin);

  if (source)
    xfree(source);

  if (dest_buf)
    xfree(dest_buf);

  xfree(outfile);
  xfree(infile);
  hashmap_free(defineopts);
  dynarray_free_deep(g_includeopts);

  mmgr_finish(getenv("MEMSTAT") != NULL);

  return ret;
}

#include <ctype.h>
#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

#include "asm/libasm.h"
#include "common/mmgr.h"

static void print_usage(char *cmd) {
  printf("Z80 disssembler\n"
         "treats given binary file as z80 binary code and dumps disassembly to stdout\n\n"
         "usage: %s [options] <input file>\n\n"
         "options:\n"
         "  -h              this help\n"
         "  -a              don't add instruction address in the line comment (default: do)\n"
         "  -s              don't add instruction source in the line comment (default: do)\n"
         "  -l              don't add symbolic labels for jumps and calls (default: do)\n"
         "  -t              don't add timing value per instruction (default: do)\n"
         "  -o <org_addr>   set start address (ORG directive) for this code (default: 0)\n",
         cmd
  );
}

static bool parse_hexdec(char *strval, int *ival) {
  int base;
  int len = strlen(strval);
  char *last = strval + len;

  if (strval == NULL || len == 0)
    return false;

  base = 10;
  if (tolower(strval[len - 1]) == 'h') {
    base = 16;
    last--;
  }

  char *endptr;
  int tmp = strtol(strval, &endptr, base);

  if (endptr != last)
    return false;

  *ival = tmp;
  return true;
}

int main(int argc, char **argv) {
  int optflag;
  char *infile = NULL;
  size_t ret;
  FILE *fin = NULL;
  struct libasm_disas_desc_t desc;

  mmgr_init();

  memset(&desc, 0, sizeof(desc));

  desc.opt_addr = true;
  desc.opt_source = true;
  desc.opt_labels = true;
  desc.opt_timings = true;
  desc.org = 0;

  opterr = 0;

  while ((optflag = getopt(argc, argv, "haslto:")) != -1) {
    switch (optflag) {
      case 'a':
        desc.opt_addr = false;
        break;

      case 's':
        desc.opt_source = false;
        break;

      case 'l':
        desc.opt_labels = false;
        break;

      case 't':
        desc.opt_timings = false;
        break;

      case 'o': {
        if (!parse_hexdec(optarg, &desc.org)) {
          fprintf(stderr, "error parsing integer value %s\n", optarg);
          return 1;
        }
        break;
      }

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

  fin = fopen(infile, "r");
  if (!fin) {
    perror(infile);
    goto out;
  }

  fseek(fin, 0, SEEK_END);
  desc.data_size = ftell(fin);
  fseek(fin, 0, SEEK_SET);


  desc.data = (char *)xmalloc(desc.data_size);

  ret = fread(desc.data, desc.data_size, 1, fin);
  if (ret != 1) {
    perror(infile);
    goto out;
  }

  char *result = libasm_disas(&desc);
  printf("%s\n", result);
  xfree(result);

out:
  if (fin)
    fclose(fin);

  if (desc.data)
    xfree(desc.data);

  xfree(infile);

  mmgr_finish(getenv("MEMSTAT") != NULL);

  return 0;
}

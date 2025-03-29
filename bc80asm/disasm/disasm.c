#include <ctype.h>
#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

#include "bits/mmgr.h"
#include "disasm/disasm.h"

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
  char *data;
  uint32_t data_size;
  bool opt_addr = true;
  bool opt_source = true;
  bool opt_labels = true;
  bool opt_timings = true;
  int opt_org = 0;

  mmgr_init();

  opterr = 0;

  while ((optflag = getopt(argc, argv, "haslto:")) != -1) {
    switch (optflag) {
      case 'a':
        opt_addr = false;
        break;

      case 's':
        opt_source = false;
        break;

      case 'l':
        opt_labels = false;
        break;

      case 't':
        opt_timings = false;
        break;

      case 'o': {
        if (!parse_hexdec(optarg, &opt_org)) {
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
  data_size = ftell(fin);
  fseek(fin, 0, SEEK_SET);


  data = (char *)xmalloc(data_size);

  ret = fread(data, data_size, 1, fin);
  if (ret != 1) {
    perror(infile);
    goto out;
  }

  char *result = disassemble(data, data_size,
                                opt_addr,
                                opt_source,
                                opt_labels,
                                opt_timings,
                                opt_org);

  printf("%s\n", result);
  xfree(result);

out:
  if (fin)
    fclose(fin);

  if (data)
    xfree(data);

  xfree(infile);

  mmgr_finish(getenv("MEMSTAT") != NULL);

  return 0;
}

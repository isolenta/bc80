#include <stdio.h>
#include <stdbool.h>
#include <stdint.h>
#include <string.h>
#include <stdlib.h>
#include <unistd.h>
#include <ctype.h>

#include "mmgr.h"

int main(int argc, char **argv) {
  char *infile = NULL;

  mmgr_init();

  if (argc > 1)
    infile = xstrdup(argv[optind]);

  if (!infile) {
    fprintf(stderr, "usage: %s <input_file>\n", argv[0]);
    return 1;
  }

  // TODO: actual work

  xfree(infile);
  mmgr_finish(getenv("MEMSTAT") != NULL);

  return 0;
}
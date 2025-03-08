#include <assert.h>
#include <setjmp.h>
#include <stdio.h>

#include "asm/compile.h"
#include "asm/disas.h"
#include "asm/parse.h"

int libasm_as(struct libasm_as_desc_t *desc) {
  dynarray *parse = NULL;
  int result;

  assert(desc);

  result = parse_source(desc, &parse);
  if (result != 0)
    return result;

  result = compile(desc, parse);
  return result;
}

char *libasm_disas(struct libasm_disas_desc_t *desc) {
  return disassemble(desc->data,
                      desc->data_size,
                      desc->opt_addr,
                      desc->opt_source,
                      desc->opt_labels,
                      desc->opt_timings,
                      desc->org);
}

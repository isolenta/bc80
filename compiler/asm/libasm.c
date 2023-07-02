#include <stdio.h>
#include <string.h>
#include <unistd.h>
#include <sys/stat.h>
#include <sys/errno.h>
#include <setjmp.h>

#include "common.h"
#include "dynarray.h"
#include "hashmap.h"
#include "parse.h"
#include "compile.h"
#include "filesystem.h"
#include "disas.h"

#include "libasm.h"

static jmp_buf parse_env;
static jmp_buf compile_env;

int libasm_as(struct libasm_as_desc_t *desc) {
  dynarray *parse = NULL;
  int result;

  assert(desc);

  result = setjmp(parse_env);
  if (result != 0)
    // returning from longjmp (error handler in parser)
    return result;

  result = parse_string(desc, &parse, &parse_env);
  if (result != 0)
    return result;

  result = setjmp(compile_env);
  if (result != 0)
    // returning from longjmp (error handler in compiler)
    return result;

  result = compile(desc, parse, &compile_env);
  return result;
}

char *libasm_disas(struct libasm_disas_desc_t *desc) {
  return disassemble(desc->data,
                      desc->data_size,
                      desc->opt_addr,
                      desc->opt_source,
                      desc->opt_labels,
                      desc->org);
}

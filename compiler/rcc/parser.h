#pragma once

#include <stdbool.h>

typedef enum IntegerBase
{
  HEX = 0,
  DEC
} IntegerBase;

typedef struct rcc_ctx_t rcc_ctx_t;

struct scanner_state {
  rcc_ctx_t *rcc_ctx;
  bool skipped;
  int line_num;
  int pos_num;
  bool scan_standalone;   // true: at preprocessor stage; false: controlled by parser
  char *string_literal;
  int int_literal;
  int num_comment_lines;
  char *whitespace_str;
  char *source_ptr;
};

struct parser_state {
  int last_pp_line;
  int last_actual_line;
};

extern int parse_int(const char *str, int len, IntegerBase base);

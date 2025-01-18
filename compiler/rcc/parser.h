#pragma once

#include <stdbool.h>

typedef enum IntegerBase
{
  HEX = 0,
  DEC
} IntegerBase;

struct scanner_state {
  bool skipped;
  int line_num;
  bool nl_from_scanner;
  char *string_literal;
};

struct parser_state {
  int last_pp_line;
  int last_actual_line;
};

extern int parse_int(const char *str, int len, IntegerBase base);

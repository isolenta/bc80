#pragma once

#include <stdbool.h>

struct scanner_state {
  bool skipped;
  int line_num;
  int pos_num;
};

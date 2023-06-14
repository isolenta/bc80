#include <string.h>
#include <stdlib.h>
#include <stdio.h>
#include <stdarg.h>
#include <ctype.h>

#include "common.h"
#include "dynarray.h"
#include "parse.h"

dynarray *split_string_sep(char *str, char sep, bool first) {
  dynarray *darray = NULL;
  char *tmpstr = strdup(str);
  char *p = tmpstr;
  char *token = p;

  while (*p) {
    if (*p == sep) {
      *p = 0;
      darray = dynarray_append_ptr(darray, strdup(token));
      token = p + 1;

      if (first)
        break;
    }
    p++;
  }
  darray = dynarray_append_ptr(darray, strdup(token));

  free(tmpstr);
  return darray;
}

bool parse_any_integer(char *strval, int *ival) {
  int base = 10;
  int len = strlen(strval);
  char *last = strval + len;

  if (strval == NULL || len == 0)
    return false;

  if (tolower(strval[len - 1]) == 'd') {
    base = 10;
    last--;
  } else if (tolower(strval[len - 1]) == 'h') {
    base = 16;
    last--;
  } else if (tolower(strval[len - 1]) == 'o') {
    base = 8;
    last--;
  } else if (strval[0] == '0') {
    base = 8;
  }

  char *endptr;
  int tmp = strtol(strval, &endptr, base);

  if (endptr != last)
    return false;

  *ival = tmp;
  return true;
}

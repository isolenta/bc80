#include <ctype.h>
#include <string.h>

#include "common/dynarray.h"
#include "common/mmgr.h"

dynarray *split_string_sep(char *str, char sep, bool first) {
  dynarray *darray = NULL;
  char *tmpstr = xstrdup(str);
  char *p = tmpstr;
  char *token = p;

  while (*p) {
    if (*p == sep) {
      *p = 0;
      darray = dynarray_append_ptr(darray, xstrdup(token));
      token = p + 1;

      if (first)
        break;
    }
    p++;
  }
  darray = dynarray_append_ptr(darray, xstrdup(token));

  xfree(tmpstr);
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

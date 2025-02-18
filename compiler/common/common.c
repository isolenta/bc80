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

char *get_token_at(char *source, int line, int pos, int token_len)
{
  int i;
  char *p = source;

  // move to desired line
  line--;
  while (*p != '\0' && line > 0) {
    if (p[0] == '\n')
      line--;

    p++;
  }

  // move to token start
  for (i = 0; i < pos - 1; i++) {
    if (*p == '\0') {
      return NULL;
    }
    p++;
  }

  char *result = xmalloc(token_len + 1);
  for (i = 0; i < token_len; i++) {
    if (*p == '\0')
      ;
    result[i] = *p;
    p++;
  }

  result[i] = '\0';

  return result;
}

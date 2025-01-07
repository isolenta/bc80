#pragma once

#include <assert.h>
#include <limits.h>
#include <stdbool.h>
#include <stddef.h>
#include <stdlib.h>

#define Max(x, y)   ((x) > (y) ? (x) : (y))

/*
 *    Returns the position of the most significant set bit in "word",
 *    measured from the least significant bit.  word must not be 0.
 */
static inline int
leftmost_one_pos32(uint32_t word)
{
  assert(word != 0);
  return 31 - __builtin_clz(word);
}

/*
 *    Returns the next higher power of 2 above 'num', or 'num' if it's
 *    already a power of 2.
 *
 * 'num' mustn't be 0 or be above PG_UINT32_MAX / 2 + 1.
 */
static inline uint32_t nextpower2_32(uint32_t num)
{
  assert(num > 0 && num <= UINT32_MAX / 2 + 1);

  /*
   * A power 2 number has only 1 bit set.  Subtracting 1 from such a number
   * will turn on all previous bits resulting in no common bits being set
   * between num and num-1.
   */
  if ((num & (num - 1)) == 0)
    return num;       /* already power 2 */

  return ((uint32_t) 1) << (leftmost_one_pos32(num) + 1);
}

typedef struct dynarray dynarray;


extern dynarray *split_string_sep(char *str, char sep, bool first);
extern bool parse_any_integer(char *strval, int *ival);
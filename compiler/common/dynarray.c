#include <string.h>

#include "common/dynarray.h"
#include "common/mmgr.h"

// Overhead for the fixed part of a dynarray header, measured in dynarray_cells
#define DYNARRAY_HEADER_OVERHEAD  \
  ((int) ((offsetof(dynarray, initial_elements) - 1) / sizeof(dynarray_cell) + 1))

// Locate the n'th cell (counting from 0) of the dynarray. It is an assertion failure if there is no such cell
dynarray_cell *dynarray_nth_cell(const dynarray *darray, int n)
{
  assert(darray != NULL);
  assert(n >= 0 && n < darray->length);
  return &darray->elements[n];
}

// Return the last cell in a non-NULL dynarray
static dynarray_cell *dynarray_last_cell(const dynarray *darray)
{
  assert(darray != NULL);
  return &darray->elements[darray->length - 1];
}


// Return the pointer value contained in the n'th element of the specified dynarray (dynarray elements begin at 0)
static void *dynarray_nth(const dynarray *darray, int n)
{
  assert(DYNARRAY_IS_PTR(darray));
  return dfirst(dynarray_nth_cell(darray, n));
}

// Return the integer value contained in the n'th element of the specified dynarray
static int dynarray_nth_int(const dynarray *darray, int n)
{
  assert(DYNARRAY_IS_INT(darray));
  return dfirst_int(dynarray_nth_cell(darray, n));
}


// Return a freshly allocated dynarray with room for at least min_size cells.
// Since empty non-NULL dynarrays are invalid, new_dynarray() sets the initial length
// to min_size, effectively marking that number of cells as valid; the caller
// is responsible for filling in their data.
static dynarray *new_dynarray(int type, int min_size)
{
  dynarray *darray;
  int max_size;

  assert(min_size > 0);

  // We allocate all the requested cells, and possibly some more, as part of
  // the same palloc request as the dynarray header.  This is a big win for the
  // typical case of short fixed-length arrays.  It can lose if we allocate a
  // moderately long array and then it gets extended; we'll be wasting more
  // initial_elements[] space than if we'd made the header small.  However,
  // rounding up the request as we do in the normal code path provides some
  // defense against small extensions.

  // Normally, we set up an array with some extra cells, to allow it to grow
  // without a realloc.  Prefer cell counts chosen to make the total
  // allocation a power-of-2, since malloc would round it up to that anyway.
  // (That stops being true for very large allocations, but very long arrays
  // are infrequent, so it doesn't seem worth special logic for such cases.)
  // The minimum allocation is 8 cell units, providing either 4 or 5
  // available cells depending on the machine's word width.  Counting
  // malloc's overhead, this uses the same amount of space as a one-cell
  // array did in the old implementation, and less space for any longer arrays.
  // We needn't worry about integer overflow; no caller passes min_size
  // that's more than twice the size of an existing array, so the size limits
  // within malloc will ensure that we don't overflow here.

  max_size = nextpower2_32(Max(8, min_size + DYNARRAY_HEADER_OVERHEAD));
  max_size -= DYNARRAY_HEADER_OVERHEAD;

  darray = (dynarray *) xmalloc(offsetof(dynarray, initial_elements) +
                max_size * sizeof(dynarray_cell));
  darray->type = type;
  darray->length = min_size;
  darray->max_length = max_size;
  darray->elements = darray->initial_elements;

  return darray;
}

// Enlarge an existing non-NULL dynarray to have room for at least min_size cells.
// This does *not* update dynarray->length, as some callers would find that
// inconvenient.  (dynarray->length had better be the correct number of existing
// valid cells, though.)
static void
enlarge_dynarray(dynarray *darray, int min_size)
{
  int new_max_len;

  assert(min_size > darray->max_length);

  // we prefer power-of-two total allocations; but here we need not account for array header overhead.
  // clamp the minimum value to 16, a semi-arbitrary small power of 2
  new_max_len = nextpower2_32(Max(16, min_size));

  if (darray->elements == darray->initial_elements)
  {
    // Replace original in-line allocation with a separate malloc block.
    darray->elements = (dynarray_cell *)xmalloc(new_max_len * sizeof(dynarray_cell));
    memcpy(darray->elements, darray->initial_elements, darray->length * sizeof(dynarray_cell));
  }
  else
  {
    darray->elements = (dynarray_cell *) xrealloc(darray->elements, new_max_len * sizeof(dynarray_cell));
  }

  darray->max_length = new_max_len;
}

// Make room for a new tail cell in the given (non-NULL) array.
// The data in the new tail cell is undefined; the caller should be
// sure to fill it in
static void
new_tail_cell(dynarray *darray)
{
  // enlarge array if necessary
  if (darray->length >= darray->max_length)
    enlarge_dynarray(darray, darray->length + 1);
  darray->length++;
}

// Append a pointer to the specified dynarray
dynarray *dynarray_append_ptr(dynarray *darray, void *value)
{
  assert(DYNARRAY_IS_PTR(darray));

  if (darray == NULL)
    darray = new_dynarray(DYNARRAY_TYPE_PTR, 1);
  else
    new_tail_cell(darray);

  dlast(darray) = value;

  return darray;
}

// Append an integer to the specified dynarray
dynarray *dynarray_append_int(dynarray *darray, int value)
{
  assert(DYNARRAY_IS_INT(darray));

  if (darray == NULL)
    darray = new_dynarray(DYNARRAY_TYPE_INT, 1);
  else
    new_tail_cell(darray);

  dlast_int(darray) = value;

  return darray;
}


// Free all storage in a dynarray, and optionally the pointed-to elements
static void dynarray_free_private(dynarray *darray, bool deep)
{
  if (darray == NULL)
    return;

  if (deep)
  {
    for (int i = 0; i < darray->length; i++)
      xfree(dfirst(&darray->elements[i]));
  }
  if (darray->elements != darray->initial_elements)
    xfree(darray->elements);
  xfree(darray);
}

// Free all the cells of the dynarray, as well as the dynarray itself. Any
// objects that are pointed-to by the cells of the dynarray are NOT free'd.

// On return, the argument to this function has been freed, so the
// caller would be wise to set it to NULL for safety's sake.
void dynarray_free(dynarray *darray)
{
  dynarray_free_private(darray, false);
}

 // Free all the cells of the dynarray, the dynarray itself, and all the
 // objects pointed-to by the cells of the dynarray (each element in the
 // dynarray must contain a pointer to a malloc()'d region of memory!)

 // On return, the argument to this function has been freed, so the
 // caller would be wise to set it to NULL for safety's sake.
void dynarray_free_deep(dynarray *darray)
{
  assert(DYNARRAY_IS_PTR(darray));
  dynarray_free_private(darray, true);
}

int dynarray_length(const dynarray *d)
{
  return d ? d->length : 0;
}

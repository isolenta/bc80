#pragma once

#include "common/common.h"

#define DYNARRAY_TYPE_PTR 1
#define DYNARRAY_TYPE_INT 2

#define DYNARRAY_IS_PTR(d) ((d) == NULL || ((d)->type == DYNARRAY_TYPE_PTR))
#define DYNARRAY_IS_INT(d) ((d) == NULL || ((d)->type == DYNARRAY_TYPE_INT))

typedef union dynarray_cell
{
  void *ptr_value;
  int  int_value;
} dynarray_cell;

typedef struct dynarray
{
  int type;                           // one of DYNARRAY_TYPE_XXX
  int length;                         // number of elements currently present
  int max_length;                     // allocated length of elements[]
  dynarray_cell *elements;            // re-allocatable array of cells
  dynarray_cell initial_elements[0];  // If elements == initial_elements, it's not a separate allocation
} dynarray;

typedef struct foreachstate
{
  const dynarray *d;
  int i;
} foreachstate;

#define dfirst(cell)          ((cell)->ptr_value)
#define dfirst_int(cell)      ((cell)->int_value)

#define dsecond(d)        dfirst(dynarray_nth_cell(d, 1))
#define dsecond_int(d)      dfirst_int(dynarray_nth_cell(d, 1))

#define dinitial(d)         dfirst(dynarray_nth_cell(d, 0))
#define dinitial_int(d)     dfirst_int(dynarray_nth_cell(d, 0))

#define dlast(d)            dfirst(dynarray_last_cell(d))
#define dlast_int(d)        dfirst_int(dynarray_last_cell(d))


// foreach - a convenience macro for looping through a dynarray

// "cell" must be the name of a "dynarray_cell *" variable; it's made to point
// to each array element in turn.  "cell" will be NULL after normal exit from
// the loop, but an early "break" will leave it pointing at the current
// dynarray element.

// Beware of changing the array object while the loop is iterating.
// The current semantics are that we examine successive array indices in
// each iteration, so that insertion or deletion of array elements could
// cause elements to be re-visited or skipped unexpectedly. However, it's safe
// to append elements to the dynarray (or in general, insert them after the
// current element); such new elements are guaranteed to be visited.

// Also, the current element of the dynarray can be deleted, if you use
// foreach_delete_current() to do so.  BUT: either of these actions will
// invalidate the "cell" pointer for the remainder of the current iteration.
#define foreach(cell, darray)  \
  for (foreachstate cell##__state = {(darray), 0}; \
     (cell##__state.d != NULL && \
      cell##__state.i < cell##__state.d->length) ? \
     (cell = &cell##__state.d->elements[cell##__state.i], true) : \
     (cell = NULL, false); \
     cell##__state.i++)

#define foreach_current_index(cell)  (cell##__state.i)

extern dynarray *dynarray_append_ptr(dynarray *darray, void *value);
extern dynarray *dynarray_append_int(dynarray *darray, int value);
extern void dynarray_free(dynarray *darray);
extern void dynarray_free_deep(dynarray *darray);
extern int dynarray_length(const dynarray *d);
extern dynarray_cell *dynarray_nth_cell(const dynarray *darray, int n);
extern dynarray_cell *dynarray_last_cell(const dynarray *darray);
extern void *dynarray_remove_last_cell(dynarray *darray);

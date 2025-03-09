#include <assert.h>
#include <string.h>

#include "common/hashmap.h"
#include "common/mmgr.h"

// User is responsible to allocate and free memory occupied by values.
// Hashmap only stores raw pointers and doesn't manage this memory.
// This statement doesn't apply to keys: hashmap will release keys properly.
// In the current implementation this structure will leak during update and delete.

static uint32_t hashmap_default_hash(hashmap *hm, void *key) {
  char *str = (char *)key;
  uint32_t hash = 37;

  while (*str) {
    hash = (hash * 54059) ^ (str[0] * 76963);
    str++;
  }

  return hash % 86969;
}

static void *hashmap_default_alloc(hashmap *hm, size_t size) {
  return xmalloc__(size, hm ? hm->name : "");
}

static void hashmap_default_free(hashmap *hm, void *ptr) {
  return xfree(ptr);
}

static void *hashmap_default_key_copy(hashmap *hm, void *key) {
  return (void *)xstrdup((const char *)key);
}

static int hashmap_default_key_compare(hashmap *hm, void *key1, void *key2) {
  return strcmp((const char *)key1, (const char *)key2);
}

hashmap *hashmap_create_ex(uint32_t num_entries, const char *name, alloc_fn_type alloc_fn, free_fn_type free_fn) {
  alloc_fn_type init_alloc_fn = hashmap_default_alloc;
  free_fn_type init_free_fn = hashmap_default_free;

  if (alloc_fn != NULL)
    init_alloc_fn = alloc_fn;

  if (free_fn != NULL)
    init_free_fn = free_fn;

  hashmap *hm = (hashmap *)init_alloc_fn(NULL, sizeof(hashmap));

  hm->name = name;
  hm->num_entries = num_entries;
  hm->hashtab = (hashmap_entry *)init_alloc_fn(hm, num_entries * sizeof(hashmap_entry));
  memset(hm->hashtab, 0, num_entries * sizeof(hashmap_entry));

  // default helper function for standard memory manager and cstring keys
  hm->alloc_fn = init_alloc_fn;
  hm->free_fn = init_free_fn;

  hm->key_copy_fn = hashmap_default_key_copy;
  hm->key_compare_fn = hashmap_default_key_compare;
  hm->hash_fn = hashmap_default_hash;

  return hm;
}

void hashmap_set_functions(hashmap *hm,
                            alloc_fn_type alloc_fn,
                            free_fn_type free_fn,
                            key_copy_fn_type key_copy_fn,
                            key_compare_fn_type key_compare_fn,
                            hash_fn_type hash_fn) {
  hm->alloc_fn = alloc_fn;
  hm->free_fn = free_fn;
  hm->key_copy_fn = key_copy_fn;
  hm->key_compare_fn = key_compare_fn;
  hm->hash_fn = hash_fn;
}

static void *hashmap_access_impl(hashmap *hm, void *key, int op, void *value)
{
  assert((op == HASHMAP_OP_SEARCH) || (op == HASHMAP_OP_INSERT) || (op == HASHMAP_OP_REMOVE));

  uint32_t hash = hm->hash_fn(hm, key);
  hashmap_entry *entry = &hm->hashtab[hash % hm->num_entries];

  if (entry->key == NULL) {
    // empty bucket => nothing found for this key
    if ((op == HASHMAP_OP_SEARCH) || (op == HASHMAP_OP_REMOVE))
      return NULL;

    // for HASHMAP_INSERT just place the value here
    entry->key = hm->key_copy_fn(hm, key);
    entry->value = value;
    entry->next = NULL;
    return entry->value;
  } else {
    // bucket is already occupied
    if (hm->key_compare_fn(hm, entry->key, key) == 0) {
      // we are found our key
      if (op == HASHMAP_OP_SEARCH)
        return entry->value;

      if (op == HASHMAP_OP_REMOVE) {
        void *old_value = entry->value;
        if (entry->next != NULL) {
          hashmap_entry *tmp_entry = entry->next;

          hm->free_fn(hm, entry->key);
          entry->key = tmp_entry->key;
          entry->value = tmp_entry->value;
          entry->next = tmp_entry->next;
          hm->free_fn(hm, tmp_entry);
        } else {
          hm->free_fn(hm, entry->key);
          entry->key = NULL;
          entry->value = NULL;
        }
        return old_value;
      }

      // for HASHMAP_INSERT replace value for found key
      hm->free_fn(hm, entry->value);
      entry->value = value;
      return entry->value;
    } else {
      hashmap_entry *parent = entry;

      // hash collision: continue searching our key in the bucket's list

      while (entry->next != NULL) {
        entry = entry->next;
        if (hm->key_compare_fn(hm, entry->key, key) == 0) {
          // we are found our key
          if (op == HASHMAP_OP_SEARCH) {
            return entry->value;
          }

          if (op == HASHMAP_OP_REMOVE) {
            void *old_value = entry->value;
            hm->free_fn(hm, entry->key);

            parent->next = entry->next;
            hm->free_fn(hm, entry);
            return old_value;
          }

          // for HASHMAP_INSERT replace value for found key
          entry->value = value;
          return entry->value;
        }

        parent = entry;
      }

      // nothing found in the list
      if ((op == HASHMAP_OP_SEARCH) || (op == HASHMAP_OP_REMOVE))
        return NULL;

      // for HASHMAP_INSERT allocate new entry in the list and place value here
      entry->next = hm->alloc_fn(hm, sizeof(hashmap_entry));
      entry->next->key = hm->key_copy_fn(hm, key);
      entry->next->value = value;
      entry->next->next = NULL;
      return entry->next->value;
    }
  }

  assert(0);  // never reach here
  return NULL;
}

void *hashmap_get(hashmap *hm, void *key)
{
  return hashmap_access_impl(hm, key, HASHMAP_OP_SEARCH, NULL);
}

void *hashmap_set(hashmap *hm, void *key, void *value)
{
  return hashmap_access_impl(hm, key, HASHMAP_OP_INSERT, value);
}

void *hashmap_remove(hashmap *hm, void *key)
{
  return hashmap_access_impl(hm, key, HASHMAP_OP_REMOVE, NULL);
}

void hashmap_free(hashmap *hm) {
  free_fn_type free_fn = hm->free_fn;

  for (int i = 0; i < hm->num_entries; i++) {
    hashmap_entry *entry = &hm->hashtab[i];

    free_fn(hm, entry->key);

    while (entry->next != NULL) {
      entry = entry->next;
      free_fn(hm, entry->key);
    }
  }
  free_fn(hm, hm->hashtab);
  free_fn(hm, hm);
}

hashmap_scan *hashmap_scan_init(hashmap *hm) {
  hashmap_scan *scan = (hashmap_scan *)hm->alloc_fn(hm, sizeof(hashmap_scan));

  scan->hm = hm;
  scan->current_bucket = 0;
  scan->current_entry = &scan->hm->hashtab[scan->current_bucket];

  return scan;
}

hashmap_entry *hashmap_scan_next(hashmap_scan *scan) {
  free_fn_type free_fn = scan->hm->free_fn;

  while (1) {
    if (scan->current_bucket == scan->hm->num_entries) {
      free_fn(scan->hm, scan);
      return NULL;
    }

    if (scan->current_entry != NULL && scan->current_entry->key != NULL) {
      hashmap_entry *result = scan->current_entry;

      if (scan->current_entry->next != NULL) {
        scan->current_entry = scan->current_entry->next;
      } else {
        scan->current_bucket++;
        scan->current_entry = &scan->hm->hashtab[scan->current_bucket];
      }

      return result;
    }

    if (scan->current_entry->next != NULL) {
      scan->current_entry = scan->current_entry->next;
    } else {
      scan->current_bucket++;
      scan->current_entry = &scan->hm->hashtab[scan->current_bucket];
    }
  }
}

void hashmap_scan_free(hashmap_scan *scan) {
  free_fn_type free_fn = scan->hm->free_fn;
  free_fn(scan->hm, scan);
}

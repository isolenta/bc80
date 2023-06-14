#include <stdio.h>
#include <string.h>

#include "common.h"
#include "hashmap.h"

// User is responsible to allocate and free memory occupied by values.
// Hashmap only stores raw pointers and doesn't manage this memory.
// This statement doesn't apply to keys: hashmap will release keys properly.
// In the current implementation this structure will leak during update and delete.

static uint32_t str_hash(char *str) {
  uint32_t hash = 37;

  while (*str) {
    hash = (hash * 54059) ^ (str[0] * 76963);
    str++;
  }

  return hash % 86969;
}

hashmap *hashmap_create(uint32_t num_entries) {
  hashmap *hm = (hashmap *)malloc(sizeof(hashmap));

  hm->num_entries = num_entries;
  hm->hashtab = (hashmap_entry *)malloc(num_entries * sizeof(hashmap_entry));
  memset(hm->hashtab, 0, num_entries * sizeof(hashmap_entry));

  return hm;
}

void *hashmap_search(hashmap *hm, char *key, int op, void *value) {
  assert((op == HASHMAP_FIND) || (op == HASHMAP_REMOVE) || (op == HASHMAP_INSERT));

  uint32_t hash = str_hash(key);
  hashmap_entry *entry = &hm->hashtab[hash % hm->num_entries];

  if (entry->key == NULL) {
    // empty bucket => nothing found for this key
    if ((op == HASHMAP_FIND) || (op == HASHMAP_REMOVE))
      return NULL;

    // for HASHMAP_INSERT just place the value here
    entry->key = strdup(key);
    entry->value = value;
    entry->next = NULL;
    return entry->value;
  } else {
    // bucket is already occupied
    if (strcmp(entry->key, key) == 0) {
      // we are found our key
      if (op == HASHMAP_FIND)
        return entry->value;

      if (op == HASHMAP_REMOVE) {
        if (entry->next != NULL) {
          hashmap_entry *tmp_entry = entry->next;

          free(entry->key);
          entry->key = tmp_entry->key;
          entry->value = tmp_entry->value;
          entry->next = tmp_entry->next;
          free(tmp_entry);
        } else {
          free(entry->key);
          entry->key = NULL;
          entry->value = NULL;
        }
        return NULL;
      }

      // for HASHMAP_INSERT replace value for found key
      free(entry->value);
      entry->value = value;
      return entry->value;
    } else {
      hashmap_entry *parent = entry;

      // hash collision: continue searching our key in the bucket's list

      while (entry->next != NULL) {
        entry = entry->next;
        if (strcmp(entry->key, key) == 0) {
          // we are found our key
          if (op == HASHMAP_FIND)
            return entry->value;

          if (op == HASHMAP_REMOVE) {
            free(entry->key);

            parent->next = entry->next;
            free(entry);
            return NULL;
          }

          // for HASHMAP_INSERT replace value for found key
          entry->value = value;
          return entry->value;
        }

        parent = entry;
      }

      // nothing found in the list
      if ((op == HASHMAP_FIND) || (op == HASHMAP_REMOVE))
        return NULL;

      // for HASHMAP_INSERT allocate new entry in the list and place value here
      entry->next = malloc(sizeof(hashmap_entry));
      entry->next->key = strdup(key);
      entry->next->value = value;
      entry->next->next = NULL;
      return entry->next->value;
    }
  }

  assert(0);  // never reach here
  return NULL;
}

void hashmap_free(hashmap *hm) {
  for (int i = 0; i < hm->num_entries; i++) {
    hashmap_entry *entry = &hm->hashtab[i];

    free(entry->key);

    while (entry->next != NULL) {
      entry = entry->next;
      free(entry->key);
    }
  }
  free(hm->hashtab);
  free(hm);
}

hashmap_scan *hashmap_scan_init(hashmap *hm) {
  hashmap_scan *scan = (hashmap_scan *)malloc(sizeof(hashmap_scan));

  scan->hm = hm;
  scan->current_bucket = 0;
  scan->current_entry = NULL;

  return scan;
}

hashmap_entry *hashmap_scan_next(hashmap_scan *scan) {
  while (1) {
    if ((scan->current_bucket >= scan->hm->num_entries) && (scan->current_entry == NULL)) {
      free(scan);
      return NULL;
    }

    if (scan->current_entry != NULL) {
      hashmap_entry *entry = scan->current_entry->next;
      scan->current_entry = entry;

      if (entry != NULL)
        return entry;

      scan->current_bucket++;
    }

    hashmap_entry *entry = &scan->hm->hashtab[scan->current_bucket];

    if (entry->key != NULL) {
      if (entry->next != NULL)
        scan->current_entry = entry;
      else
        scan->current_bucket++;

      return entry;
    } else {
      scan->current_bucket++;
    }
  }
}

void hashmap_scan_free(hashmap_scan *scan) {
  free(scan);
}

#pragma once

#include "common.h"

#define HASHMAP_FIND 0
#define HASHMAP_INSERT 1
#define HASHMAP_REMOVE 2

typedef struct hashmap_entry hashmap_entry;

typedef void *(*alloc_fn_type)(hashmap *hm, size_t size);
typedef void (*free_fn_type)(hashmap *hm, void *ptr);
typedef void *(*key_copy_fn_type)(hashmap *hm, void *key);
typedef int (*key_compare_fn_type)(hashmap *hm, void *key1, void *key2);
typedef uint32_t (*hash_fn_type)(hashmap *hm, void *key);

typedef struct hashmap_entry
{
  char *key;
  void *value;
  hashmap_entry *next;
} hashmap_entry;

typedef struct hashmap
{
  uint32_t num_entries;
  hashmap_entry *hashtab;

  const char *name;
  alloc_fn_type alloc_fn;
  free_fn_type free_fn;
  key_copy_fn_type key_copy_fn;
  key_compare_fn_type key_compare_fn;
  hash_fn_type hash_fn;
} hashmap;

typedef struct hashmap_scan
{
  hashmap *hm;
  uint32_t current_bucket;
  hashmap_entry *current_entry;
} hashmap_scan;

extern hashmap *hashmap_create_ex(uint32_t num_entries, const char *name, alloc_fn_type alloc_fn, free_fn_type free_fn);
#define hashmap_create(num_entries, name) hashmap_create_ex(num_entries, name, NULL, NULL)

extern void hashmap_set_functions(hashmap *hm,
                                  alloc_fn_type alloc_fn,
                                  free_fn_type free_fn,
                                  key_copy_fn_type key_copy_fn,
                                  key_compare_fn_type key_compare_fn,
                                  hash_fn_type hash_fn);
extern void *hashmap_search(hashmap *hm, void *key, int op, void *value);
extern void hashmap_free(hashmap *hm);
extern hashmap_scan *hashmap_scan_init(hashmap *hm);
extern hashmap_entry *hashmap_scan_next(hashmap_scan *scan);
extern void hashmap_scan_free(hashmap_scan *scan);

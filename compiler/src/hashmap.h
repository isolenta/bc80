#pragma once

#include "common.h"

#define HASHMAP_FIND 0
#define HASHMAP_INSERT 1
#define HASHMAP_REMOVE 2

typedef struct hashmap_entry hashmap_entry;

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
} hashmap;

typedef struct hashmap_scan
{
  hashmap *hm;
  uint32_t current_bucket;
  hashmap_entry *current_entry;
} hashmap_scan;

extern hashmap *hashmap_create(uint32_t num_entries);
extern void *hashmap_search(hashmap *hm, char *key, int op, void *value);
extern void hashmap_free(hashmap *hm);
extern hashmap_scan *hashmap_scan_init(hashmap *hm);
extern hashmap_entry *hashmap_scan_next(hashmap_scan *scan);
extern void hashmap_scan_free(hashmap_scan *scan);

#include <assert.h>
#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <stdint.h>
#include <stddef.h>
#include <stdbool.h>

#include "hashmap.h"

static hashmap *allocations = NULL;
static size_t stat_alloc_bytes = 0;
static size_t stat_dealloc_bytes = 0;
static size_t stat_max_heap = 0;
static size_t stat_num_allocs = 0;
static size_t stat_num_reallocs = 0;
static size_t stat_num_deallocs = 0;

struct alloc_entry {
  void *ptr;
  size_t size;
  const char *file;
  int line;
  const char *tag;
};

// will use standard malloc/free but not our xmalloc/xfree to exclude internal hashmap from tracking
static void *internal_alloc(hashmap *hm, size_t size) {
  return malloc(size);
}

static void internal_free(hashmap *hm, void *ptr) {
  return free(ptr);
}

static void *internal_key_copy(hashmap *hm, void *key) {
  void **newkey = internal_alloc(hm, sizeof(void*));
  void **pkey = (void **)key;
  *newkey = *pkey;
  return (void*)newkey;
}

static int internal_key_compare(hashmap *hm, void *key1, void *key2) {
  void **pkey1 = (void **)key1;
  void **pkey2 = (void **)key2;
  return (*pkey1 == *pkey2) ? 0 : -1;
}

static uint32_t internal_hash(hashmap *hm, void *key) {
  void **pkey = (void **)key;
  uint32_t hi = (uint32_t)((uintptr_t)*pkey >> 32);
  uint32_t lo = (uint32_t)((uintptr_t)*pkey & 0xffffffff);
  return hi ^ lo;
}

void mmgr_init() {
  assert(allocations == NULL);

  allocations = hashmap_create_ex(1024, "mmgr_allocations", internal_alloc, internal_free);
  hashmap_set_functions(allocations,
                        internal_alloc,
                        internal_free,
                        internal_key_copy,
                        internal_key_compare,
                        internal_hash);
}

void *xmalloc_(size_t size, const char *tag, const char *file, int line) {
  void *ptr = malloc(size);

  struct alloc_entry *entry = (struct alloc_entry *)internal_alloc(NULL, sizeof(struct alloc_entry));
  entry->ptr = ptr;
  entry->size = size;
  entry->file = file;
  entry->line = line;
  entry->tag = tag;
  hashmap_search(allocations, &ptr, HASHMAP_INSERT, entry);

  stat_alloc_bytes += size;
  stat_num_allocs++;
  if ((stat_alloc_bytes - stat_dealloc_bytes) > stat_max_heap)
    stat_max_heap = stat_alloc_bytes - stat_dealloc_bytes;

  return ptr;
}

void *xrealloc_(void *ptr, size_t size, const char *tag, const char *file, int line) {
  void *newptr = realloc(ptr, size);

  struct alloc_entry *entry = (struct alloc_entry *)hashmap_search(allocations, &ptr, HASHMAP_REMOVE, NULL);
  assert(entry);

  stat_num_allocs++;
  stat_num_deallocs++;
  stat_alloc_bytes += size;
  stat_dealloc_bytes += entry->size;
  stat_num_reallocs++;
  if ((stat_alloc_bytes - stat_dealloc_bytes) > stat_max_heap)
    stat_max_heap = stat_alloc_bytes - stat_dealloc_bytes;

  entry->ptr = newptr;
  entry->size = size;
  entry->file = file;
  entry->line = line;
  entry->tag = tag;
  hashmap_search(allocations, &newptr, HASHMAP_INSERT, entry);

  return newptr;
}

void xfree_(void *ptr, const char *file, int line) {
  (void)file;
  (void)line;

  if (ptr == NULL)
    return;

  struct alloc_entry *entry = (struct alloc_entry *)hashmap_search(allocations, &ptr, HASHMAP_REMOVE, NULL);
  assert(entry);

  stat_num_deallocs++;
  stat_dealloc_bytes += entry->size;
  if ((stat_alloc_bytes - stat_dealloc_bytes) > stat_max_heap)
    stat_max_heap = stat_alloc_bytes - stat_dealloc_bytes;

  free(ptr);
}

char *xstrdup_(const char *str, const char *file, int line) {
  if (str == NULL)
    return NULL;

  char *newstr = xmalloc_(strlen(str), "", file, line);
  strcpy(newstr, str);
  return newstr;
}

void mmgr_finish(bool dump_stats) {
  assert(allocations != NULL);

  hashmap_scan *scan = NULL;
  hashmap_entry *entry = NULL;

  if (dump_stats) {
    printf("Memory statistics:\n");
    printf("  allocs: %lu\n", stat_num_allocs);
    printf("  reallocs: %lu\n", stat_num_reallocs);
    printf("  deallocs: %lu\n", stat_num_deallocs);
    printf("  allocated: %lu bytes\n", stat_alloc_bytes);
    printf("  manually deallocated: %lu bytes\n", stat_dealloc_bytes);
    printf("  leaked: %lu bytes\n", stat_alloc_bytes - stat_dealloc_bytes);
    printf("  max heap size: %lu bytes\n", stat_max_heap);
  }

  size_t total_deallocs = 0;
  scan = hashmap_scan_init(allocations);
  while ((entry = hashmap_scan_next(scan)) != NULL) {
    struct alloc_entry *ae = (struct alloc_entry *)entry->value;
    total_deallocs++;
    free(ae->ptr);
  }

  if (dump_stats)
    printf("  post deallocs: %lu\n", total_deallocs);

  hashmap_free(allocations);
  allocations = NULL;
}
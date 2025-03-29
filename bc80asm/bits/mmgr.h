#pragma once

#include <stdbool.h>
#include <stddef.h>

extern void mmgr_init();
extern void mmgr_finish(bool dump_stats);

// use it for 'set-like' hashmaps, when value doesn't make sense but presence makes
#define XMMGR_DUMMY_PTR (void *)1

void *xmalloc_(size_t size, const char *tag, const char *file, int line);
void *xrealloc_(void *ptr, size_t size, const char *tag, const char *file, int line);
void xfree_(void *ptr, const char *file, int line);
char *xstrdup_(const char *str, const char *file, int line);

#define xmalloc2(size, name) xmalloc_(size, name, __FILE__, __LINE__)
#define xmalloc(size) xmalloc_(size, "", __FILE__, __LINE__)
#define xmalloc__(size, tag) xmalloc_(size, tag, __FILE__, __LINE__)
#define xrealloc(ptr, size)  xrealloc_(ptr, size, "", __FILE__, __LINE__)
#define xrealloc2(ptr, size, name)  xrealloc_(ptr, size, name, __FILE__, __LINE__)
#define xfree(ptr) xfree_(ptr, __FILE__, __LINE__)
#define xstrdup(str) xstrdup_(str, __FILE__, __LINE__)

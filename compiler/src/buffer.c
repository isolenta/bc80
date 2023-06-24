#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <stdarg.h>

#include "common.h"
#include "buffer.h"

#define BUFFER_INIT_SIZE 1024

buffer *buffer_init() {
  buffer *buf = (buffer *)malloc(sizeof(buffer));

  buf->maxlen = BUFFER_INIT_SIZE;
  buf->data = (char *)malloc(buf->maxlen);
  buf->data[0] = '\0';
  buf->len = 0;

  return buf;
}

void buffer_free(buffer *buf) {
  free(buf->data);
  free(buf);
}

char *buffer_dup(buffer *buf) {
  char *result;

  assert(buf != NULL);

  result = malloc(buf->len);
  memcpy(result, buf->data, buf->len);

  return result;
}

int buffer_append_va(buffer *buf, const char *fmt, va_list args)
{
  int avail;
  size_t nprinted;

  assert(buf != NULL);

  // If there's hardly any space, don't bother trying, just fail to make the
  // caller enlarge the buffer first.  We have to guess at how much to
  // enlarge, since we're skipping the formatting work.
  avail = buf->maxlen - buf->len;
  if (avail < 16)
    return 32;

  nprinted = vsnprintf(buf->data + buf->len, (size_t) avail, fmt, args);

  if (nprinted < (size_t) avail)
  {
    // Success.  Note nprinted does not include trailing null.
    buf->len += (int) nprinted;
    return 0;
  }

  // Restore the trailing null so that str is unmodified.
  buf->data[buf->len] = '\0';

  // Return vsnprintf's estimate of the space needed.  (Although this is
  // given as a size_t, we know it will fit in int because it's not more
  // than MaxAllocSize.)
  return (int) nprinted;
}

static void buffer_enlarge(buffer *buf, int needed)
{
  int newlen;

  // total space required now
  needed += buf->len + 1;

  // got enough space already
  if (needed <= buf->maxlen)
    return;

  // We don't want to allocate just a little more space with each append;
  // for efficiency, double the buffer size each time it overflows.
  // Actually, we might need to more than double it if 'needed' is big...
  newlen = 2 * buf->maxlen;
  while (needed > newlen)
    newlen = 2 * newlen;

  buf->data = (char *) realloc(buf->data, newlen);

  buf->maxlen = newlen;
}

int buffer_append(buffer *buf, const char *fmt, ...)
{
  int start = buf->len;

  for (;;)
  {
    va_list args;
    int needed;

    va_start(args, fmt);
    needed = buffer_append_va(buf, fmt, args);
    va_end(args);

    if (needed == 0)
      break;

    // Increase the buffer size and try again.
    buffer_enlarge(buf, needed);
  }

  return start;
}

int buffer_append_binary(buffer *buf, const char *data, int datalen)
{
  assert(buf != NULL);

  int start = buf->len;

  // Make more room if needed
  buffer_enlarge(buf, datalen);

  // OK, append the data
  memcpy(buf->data + buf->len, data, datalen);
  buf->len += datalen;

  return start;
}

int buffer_append_char(buffer *buf, char c)
{
  int start = buf->len;
  buffer_append_binary(buf, &c, 1);
  return start;
}

int buffer_reserve(buffer *buf, int datalen)
{
  assert(buf != NULL);

  int start = buf->len;

  // Make more room if needed
  buffer_enlarge(buf, datalen);

  buf->len += datalen;
  return start;
}

char *bsprintf(const char *fmt, ...) {
  buffer *buf = buffer_init();
  char *result;

  va_list args;

  va_start(args, fmt);
  buffer_append(buf, fmt, args);
  va_end(args);

  result = buffer_dup(buf);
  buffer_free(buf);
  return result;
}

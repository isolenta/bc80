#pragma once

typedef struct buffer
{
  char *data;
  int  len;
  int  maxlen;
} buffer;

extern buffer *buffer_init();
extern void buffer_free(buffer *str);
extern int buffer_append(buffer *str, const char *fmt, ...);
extern int buffer_append_char(buffer *str, char c);
extern int buffer_append_binary(buffer *str, const char *data, int datalen);
extern int buffer_reserve(buffer *buf, int datalen);

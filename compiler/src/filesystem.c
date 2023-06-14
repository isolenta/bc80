#include <stdlib.h>
#include <stdio.h>
#include <stdbool.h>
#include <string.h>
#include <sys/stat.h>

#include "dynarray.h"

// if filename has any suffix (something after dot symbol until end of string) replace it to new_sfx
// otherwise just add new_sfx at the end (with dot symbol)
char *fs_replace_suffix(char *filename, char *new_sfx) {
  char *result;
  int basename_len;
  char *dotpos = strrchr(filename, '.');

  if (dotpos == NULL)
    basename_len = strlen(filename);
  else
    basename_len = dotpos - filename;

  // one extra byte for dot and one extrabyte for null-terminate
  result = malloc(basename_len + 1 + strlen(new_sfx) + 1);
  memcpy(result, filename, basename_len);
  result[basename_len] = '.';
  strcpy(result + basename_len + 1, new_sfx);

  return result;
}

char *fs_get_suffix(char *filename) {
  char *dotpos = strrchr(filename, '.');
  if (dotpos == NULL)
    return "";

  char *result = malloc(strlen(filename) - (dotpos - filename));
  memcpy(result, dotpos + 1, strlen(filename) - (dotpos - filename) - 1);
  result[strlen(filename) - (dotpos - filename) - 1] = '\0';

  return result;
}

bool fs_file_exists(char *path) {
  struct stat st;
  return (stat(path, &st) == 0);
}

size_t fs_file_size(char *path) {
  struct stat st;
  int ret = stat(path, &st);

  if (ret != 0)
    return 0;

  return st.st_size;
}


// return absolute path to given filename according search rules:
//   - file exists in the current directory => <curdir>/filename
//   - file exists in the one of directories in searchlist => <dir>/filename
//   - otherwise return NULL
char *fs_abs_path(char *filename, dynarray *searchlist) {
  if (fs_file_exists(filename))
    return realpath(filename, NULL);

  dynarray_cell *dc = NULL;
  foreach(dc, searchlist) {
    char tmp[PATH_MAX];

    snprintf(tmp, PATH_MAX, "%s/%s", (char*)dfirst(dc), filename);
    if (fs_file_exists(tmp))
      return realpath(tmp, NULL);
  }

  return NULL;
}
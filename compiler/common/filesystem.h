#pragma once

#include "dynarray.h"

extern char *fs_replace_suffix(char *filename, char *new_sfx);
extern bool fs_file_exists(char *path);
extern size_t fs_file_size(char *path);
extern char *fs_abs_path(char *filename, dynarray *searchlist);
extern char *fs_get_suffix(char *filename);

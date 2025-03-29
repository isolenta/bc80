#pragma once

#include <stdbool.h>
#include <stdint.h>

typedef struct dynarray dynarray;

extern char *fs_replace_suffix(char *filename, char *new_sfx);
extern bool fs_file_exists(char *path);
extern size_t fs_file_size(char *path);
extern char *fs_abs_path(char *filename, dynarray *searchlist);
extern char *fs_get_suffix(char *filename);

extern char *read_file(char *filename);
extern void write_file(char *data, uint32_t data_size, char *filename);

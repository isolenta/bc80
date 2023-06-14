#pragma once

typedef int (*command_cb_t)(dynarray *);

typedef struct {
  char *name;
  command_cb_t callback;
} command_t;

#define NUM_COMMANDS 45

#define MAX_MULTIBYTE_BYTES 128

typedef struct dynarray dynarray;

void init_commands();
dynarray *get_tokens(char *str);
command_t *get_command(int idx);
bool check_board(char *ttyfile);
char *get_ttyfile(int argc, char **argv);
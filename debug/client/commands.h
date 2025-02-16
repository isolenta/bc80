#pragma once

typedef int (*command_cb_t)(dynarray *);

typedef struct {
  char *name;
  command_cb_t callback;
} command_t;

#define NUM_COMMANDS 40

#define MAX_MULTIBYTE_BYTES 128

typedef struct dynarray dynarray;

void init_commands();
dynarray *get_tokens(char *str);
command_t *get_command(int idx);

bool check_board(char *ttyfile);

#if __APPLE__
char *try_autodetect_board_macos();
#endif

int cmd_release(dynarray *args);

enum {
  BUS_RELEASED = 0,
  BUS_ACQUIRED,
};

extern int g_bus_state;

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdbool.h>
#include <signal.h>
#include <errno.h>
#include <assert.h>
#include <setjmp.h>
#include <pwd.h>
#include <uuid/uuid.h>
#include <unistd.h>

#include <readline/readline.h>
#include <readline/history.h>

#include "dynarray.h"
#include "rl_utils.h"
#include "commands.h"
#include "dbgproto.h"

static char *get_home_path() {
  char pwdbuf[BUFSIZ];
  struct passwd pwdstr;
  struct passwd *pwd = NULL;

  getpwuid_r(geteuid(), &pwdstr, pwdbuf, sizeof(pwdbuf), &pwd);
  if (pwd == NULL)
    return NULL;

  return strdup(pwd->pw_dir);
}

static void restore_history() {
  char history_file[1024];

  snprintf(history_file, sizeof(history_file), "%s/.bc80debug_history", get_home_path());
  read_history(history_file);
}

static void save_history() {
  char history_file[1024];

  snprintf(history_file, sizeof(history_file), "%s/.bc80debug_history", get_home_path());
  write_history(history_file);
}

static bool ctrlc_pressed = false;
static bool sigint_interrupt_enabled = false;
static sigjmp_buf  sigint_interrupt_jmp;

typedef void (*sigfunc_t)(int signo);

static void handle_sigint(int signal_arg)
{
  if (sigint_interrupt_enabled) {
    sigint_interrupt_enabled = false;
    siglongjmp(sigint_interrupt_jmp, 1);
  }

  ctrlc_pressed = true;
}

static void setsignal(int signo, sigfunc_t handler)
{
  struct sigaction act;

  act.sa_handler = handler;

  sigemptyset(&act.sa_mask);
  act.sa_flags = SA_RESTART;

  sigaction(signo, &act, NULL);
}

int main(int argc, char **argv)
{
  int result = 0;
  char *prompt;
  char *ttyfile = get_ttyfile(argc, argv);

  if (!ttyfile) {
    fprintf(stderr, "usage: debug [usb.serial.file]\n");
    exit(1);
  }

  if (!check_board(ttyfile)) {
    fprintf(stderr, "device not found at %s\n", ttyfile);
    exit(1);
  }

  // deactivate RESET and initialize internal lock state
  cmd_unlock(NULL);

  printf("successfully initialize device at %s\n", ttyfile);

  init_commands();

  setsignal(SIGINT, handle_sigint);
  prompt = strdup("bc80 # ");

  rl_initialize();
  using_history();
  rl_attempted_completion_function = command_completion;

  restore_history();
  atexit(save_history);

  while (result == 0)
  {
    char *line;
    dynarray *args = NULL;
    bool found = false;
    int cmd_retval = 0;

    rl_reset_screen_size();

    if (ctrlc_pressed)
      break;

    if (sigsetjmp(sigint_interrupt_jmp, 1) != 0) {
      putc('\n', stdout);
      continue;
    }

    sigint_interrupt_enabled = true;
    line = readline(prompt);
    sigint_interrupt_enabled = false;

    if (line != NULL) {
      if (line[0] == 0) {
        free(line);
        continue;
      }

      append_to_history(line);

      args = get_tokens(line);
    } else {
      args = dynarray_append_ptr(args, strdup("quit"));
    }

    for (int i = 0; i < NUM_COMMANDS; i++) {
      command_t *cmd = get_command(i);
      if (strcasecmp(cmd->name, (char*)dinitial(args)) == 0) {
        if (cmd->callback)
          cmd_retval = cmd->callback(args);
        found = true;
        break;
      }
    }

    if (cmd_retval == 1) {
      dynarray_free_deep(args);
      free(line);
      break;
    }

    if (!found)
      printf("%s: unknown command (type 'help' to list commands)\n", (char*)dinitial(args));

    dynarray_free_deep(args);
    free(line);
  }

  free(ttyfile);
}
#include <stdio.h>
#include <stdlib.h>

#include <readline/readline.h>
#include <readline/history.h>

#include "dynarray.h"
#include "commands.h"

// Generator function for command completion.  STATE lets us know whether
// to start from scratch; without any state (i.e. STATE == 0), then we
// start at the top of the list.
static char *complete_command_name(const char *text, int state)
{
  static int list_index, len;

  // if this is a new word to complete, initialize now.  This includes
  // saving the length of TEXT for efficiency, and initializing the index variable to 0.
  if (!state) {
    list_index = 0;
    len = strlen (text);
  }

  // return the next name which partially matches from the command list.
  while (1) {
    command_t *cmd;

    if (list_index >= NUM_COMMANDS)
      return NULL;

    cmd = get_command(list_index);
    list_index++;

    if (strncmp(cmd->name, text, len) == 0)
      return strdup(cmd->name);
  }

  return NULL;
}

static char *
complete_nothing(const char *text, int state)
{
  if (state == 0)
    return strdup("");
  else
    return NULL;
}

char **command_completion(const char *text, int start, int end)
{
  char **matches = NULL;

  // if this word is at the start of the line, then it is a command to complete
  if (start == 0)
    matches = rl_completion_matches(text, complete_command_name);

  // if nothing matches complete to empty string to prevent completion with filenames
  // and prevent readline from appending stuff to the non-match
  if (matches == NULL) {
    matches = rl_completion_matches(text, complete_nothing);
    rl_completion_append_character = '\0';
  }

  return matches;
}

void append_to_history(char *line)
{
  static char *prev_history_line = NULL;
  char *s = line;
  int i;

  // trim any trailing \n's
  for (i = strlen(s) - 1; i >= 0 && s[i] == '\n'; i--)
    ;

  s[i + 1] = '\0';

  // don't add empty (or whitespace-only) lines to the history; prevent duplicates as well
  if (s[0] && (s[0] != ' ') && ((prev_history_line == NULL) || (prev_history_line && (strcmp(s, prev_history_line) != 0))))
  {
    if (prev_history_line)
      free(prev_history_line);

    prev_history_line = strdup(s);

    add_history(s);
  }
}


#include "board.h"
#include <cc1101_commands.h>
#include <cc1101_func.h>
#include <shell.h>

static int cc1101_init_handler(printf_func pfunc, gets_func gfunc, int argc, char **argv, void *data);
static const ShellCommandItem cc1101_init_command_items[] = {
  {NULL, NULL, cc1101_init_handler}
};
static const ShellCommand cc1101_init_command = {
  cc1101_init_command_items,
  "cc1101_init",
  "cc1101_init",
  NULL,
  NULL
};

static int cc1101_init_handler(printf_func pfunc, gets_func gfunc, int argc, char **argv, void *data)
{
  if (!cc1101Init())
  {
    pfunc("cc1101 init failed\r\n");
    return 1;
  }
  return 0;
}

void register_cc1101_commands(void)
{
  shell_register_command(&cc1101_init_command);
}


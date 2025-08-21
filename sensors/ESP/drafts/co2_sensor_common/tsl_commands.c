#include "board.h"
#include <tsl_commands.h>
#include <shell.h>
#include <tsl2591.h>

static const tsl2591_config config = {
  .integration_time_ms = 600,
  .als_interrupt_enable = 0,
  .als_high_threshold = 0,
  .als_low_threshold = 0,
  .no_persist_interrupt_enable = 1,
  .no_persist_als_low_threshold = 1,
  .no_persist_als_high_threshold = 5,
  .persistence_filter = AnyOutOfRange,
  .sleep_after_interrupt = 0
};

static int tsl_init_handler(printf_func pfunc, gets_func gfunc, int argc, char **argv, void *data);
static const ShellCommandItem tsl_init_command_items[] = {
  {NULL, NULL, tsl_init_handler}
};
static const ShellCommand tsl_init_command = {
  tsl_init_command_items,
  "tsl_init",
  "tsl_init",
  NULL,
  NULL
};

static int tsl_get_handler(printf_func pfunc, gets_func gfunc, int argc, char **argv, void *data);
static const ShellCommandItem tsl_get_command_items[] = {
  {NULL, NULL, tsl_get_handler}
};
static const ShellCommand tsl_get_command = {
  tsl_get_command_items,
  "tsl_get",
  "tsl_get",
  NULL,
  NULL
};

static int tsl_get_handler(printf_func pfunc, gets_func gfunc, int argc, char **argv, void *data)
{
  tsl2591_result result;
  int rc = tsl2591_measure(&result);
  if (rc)
    return rc;
  pfunc("luminocity: %f gain %d tries %d\n", result.lux, result.gain, result.tries);
  return 0;
}

static int tsl_init_handler(printf_func pfunc, gets_func gfunc, int argc, char **argv, void *data)
{
  return tsl2591_init(&config);
}

void register_tsl_commands(void)
{
  shell_register_command(&tsl_init_command);
  shell_register_command(&tsl_get_command);
}

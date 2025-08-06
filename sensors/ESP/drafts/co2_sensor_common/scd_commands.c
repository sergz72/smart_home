#include "board.h"
#include <scd_commands.h>
#include <shell.h>
#include <scd30.h>

static int scd_init_handler(printf_func pfunc, gets_func gfunc, int argc, char **argv, void *data);
static const ShellCommandItem scd_init_command_items[] = {
  {NULL, param_handler, NULL},
  {NULL, NULL, scd_init_handler}
};
static const ShellCommand scd_init_command = {
  scd_init_command_items,
  "scd_init",
  "scd_init",
  NULL,
  NULL
};

static int scd_get_handler(printf_func pfunc, gets_func gfunc, int argc, char **argv, void *data);
static const ShellCommandItem scd_get_command_items[] = {
  {NULL, NULL, scd_get_handler}
};
static const ShellCommand scd_get_command = {
  scd_get_command_items,
  "scd_get",
  "scd_get",
  NULL,
  NULL
};

static int scd_get_handler(printf_func pfunc, gets_func gfunc, int argc, char **argv, void *data)
{
  scd30_result result;
  unsigned int rc = scd30_measure(&result);
  if (rc)
    return (int)rc;
  pfunc("CO2: %f\ntemperature: %f\nhumidity: %f\n", result.co2, result.temperature, result.humidity);
  return 0;
}

static int scd_init_handler(printf_func pfunc, gets_func gfunc, int argc, char **argv, void *data)
{
  int interval = atoi(argv[0]);
  if (interval < 2 || interval > 1800)
  {
    pfunc("invalid interval");
    return 1;
  }
  return scd30_init_sensor(interval);
}

void register_scd_commands(void)
{
  shell_register_command(&scd_init_command);
  shell_register_command(&scd_get_command);
}

#include <vl50_commands.h>
#include "board.h"
#include <shell.h>
#include <string.h>
#include <VL53L0X.h>

static int vl_init_handler(printf_func pfunc, gets_func gfunc, int argc, char **argv, void *data);
static const ShellCommandItem vl_init_command_items[] = {
  {NULL, NULL, vl_init_handler}
};
static const ShellCommand vl_init_command = {
  vl_init_command_items,
  "vl50_init",
  "vl50_init",
  NULL,
  NULL
};

static int vl_measure_handler(printf_func pfunc, gets_func gfunc, int argc, char **argv, void *data);
static const ShellCommandItem vl_measure_command_items[] = {
  {NULL, param_handler, NULL},
  {NULL, NULL, vl_measure_handler}
};
static const ShellCommand vl_measure_command = {
  vl_measure_command_items,
  "vl50_measure",
  "vl50_measure timing_budget",
  NULL,
  NULL
};

static int vl_init_handler(printf_func pfunc, gets_func gfunc, int argc, char **argv, void *data)
{
  gpio_set_level(PIN_VL53L0_XSCHUT, 1);
  delayms(20);
  return VL53L0x_init(1);
}

static int vl_measure_handler(printf_func pfunc, gets_func gfunc, int argc, char **argv, void *data)
{
  int timing_budget = atoi(argv[0]);
  if (timing_budget < 20 || timing_budget > 1000)
  {
    pfunc("Incorrect timing budget");
    return 1;
  }
  int rc = VL53L0x_setMeasurementTimingBudget(timing_budget * 1000);
  if (rc)
    return rc;
  unsigned short distance;
  rc = VL53L0x_readRangeSingleMillimeters(&distance);
  if (rc)
    return rc;
  pfunc("distance: %d\r\n", distance);
  return 0;
}

void register_vl50_commands(void)
{
  shell_register_command(&vl_init_command);
  shell_register_command(&vl_measure_command);
}

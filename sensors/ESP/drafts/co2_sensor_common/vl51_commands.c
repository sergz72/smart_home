#include <vl51_commands.h>
#include "board.h"
#include <shell.h>
#include <string.h>
#include <vl_sensor.h>
#include <VL53L1X.h>

static int vl_init_handler(printf_func pfunc, gets_func gfunc, int argc, char **argv, void *data);
static const ShellCommandItem vl_init_command_items[] = {
  {NULL, param_handler, NULL},
  {NULL, param_handler, NULL},
  {NULL, NULL, vl_init_handler}
};
static const ShellCommand vl_init_command = {
  vl_init_command_items,
  "vl51_init",
  "vl51_init distance timing_budget",
  NULL,
  NULL
};

static int vl_measure_handler(printf_func pfunc, gets_func gfunc, int argc, char **argv, void *data);
static const ShellCommandItem vl_measure_command_items[] = {
  {NULL, NULL, vl_measure_handler}
};
static const ShellCommand vl_measure_command = {
  vl_measure_command_items,
  "vl51_measure",
  "vl51_measure",
  NULL,
  NULL
};

static int vl_init_handler(printf_func pfunc, gets_func gfunc, int argc, char **argv, void *data)
{
#ifdef PIN_VL53L1_XSCHUT
  DistanceMode dm = Long;
  if (!strcmp("short", argv[0]))
    dm = Short;
  else if (!strcmp("medium", argv[0]))
    dm = Medium;
  else if (strcmp("long", argv[0]))
  {
    pfunc("invalid distance mode\r\n");
    return 1;
  }
  int timing_budget = atoi(argv[1]);
  if (timing_budget < 20 || timing_budget > 1000)
  {
    pfunc("invalid timing budget\r\n");
    return 1;
  }
  gpio_set_level(PIN_VL53L1_XSCHUT, 1);
  delayms(20);
  return VL53L1X_init(1, dm, timing_budget * 1000, 5000);
#else
  return 1;
#endif
}

static int vl_measure_handler(printf_func pfunc, gets_func gfunc, int argc, char **argv, void *data)
{
  unsigned short distance = VLMeasureDistance();
  pfunc("distance: %d\r\n", distance);
  return 0;
}

void register_vl51_commands(void)
{
  shell_register_command(&vl_init_command);
  shell_register_command(&vl_measure_command);
}

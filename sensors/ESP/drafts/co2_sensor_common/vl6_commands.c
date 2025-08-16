#include <vl6_commands.h>
#include "board.h"
#include <shell.h>
#include <vl6180.h>

static vl6180_cfg cfg = {
  8, //als_int_mode: Interrupt mode source for Als readings: Level Low (value < thresh_low)
  0, // unused
  0, // unused
  0xFF, // unused
  10,
  10,
  100, // 1s
  100, // ms
  6
};

static int vl_init_handler(printf_func pfunc, gets_func gfunc, int argc, char **argv, void *data);
static const ShellCommandItem vl_init_command_items[] = {
  {NULL, NULL, vl_init_handler}
};
static const ShellCommand vl_init_command = {
  vl_init_command_items,
  "vl6_init",
  "vl6_init",
  NULL,
  NULL
};

static int vl_als_status_handler(printf_func pfunc, gets_func gfunc, int argc, char **argv, void *data);
static const ShellCommandItem vl_als_status_command_items[] = {
  {NULL, NULL, vl_als_status_handler}
};
static const ShellCommand vl_als_status_command = {
  vl_als_status_command_items,
  "vl6_als_status",
  "vl6_als_status",
  NULL,
  NULL
};

static int vl_interrupt_clear_handler(printf_func pfunc, gets_func gfunc, int argc, char **argv, void *data);
static const ShellCommandItem vl_interrupt_clear_command_items[] = {
  {NULL, NULL, vl_interrupt_clear_handler}
};
static const ShellCommand vl_interrupt_clear_command = {
  vl_interrupt_clear_command_items,
  "vl6_interrupt_clear",
  "vl6_interrupt_clear",
  NULL,
  NULL
};

static int vl_als_start_handler(printf_func pfunc, gets_func gfunc, int argc, char **argv, void *data);
static const ShellCommandItem vl_als_start_command_items[] = {
  {NULL, param_handler, NULL},
  {NULL, NULL, vl_als_start_handler}
};
static const ShellCommand vl_als_start_command = {
  vl_als_start_command_items,
  "vl6_als_start",
  "vl6_als_start flags",
  NULL,
  NULL
};

static int vl_als_get_handler(printf_func pfunc, gets_func gfunc, int argc, char **argv, void *data);
static const ShellCommandItem vl_als_get_command_items[] = {
  {NULL, param_handler, NULL},
  {NULL, NULL, vl_als_get_handler}
};
static const ShellCommand vl_als_get_command = {
  vl_als_get_command_items,
  "vl6_als_get",
  "vl6_als_get coefficient",
  NULL,
  NULL
};

static int vl_als_set_gain_handler(printf_func pfunc, gets_func gfunc, int argc, char **argv, void *data);
static const ShellCommandItem vl_als_set_gain_command_items[] = {
  {NULL, param_handler, NULL},
  {NULL, NULL, vl_als_set_gain_handler}
};
static const ShellCommand vl_als_set_gain_command = {
  vl_als_set_gain_command_items,
  "vl6_als_set_gain",
  "vl6_als_set_gain gain",
  NULL,
  NULL
};

static int vl_als_set_iperiod_handler(printf_func pfunc, gets_func gfunc, int argc, char **argv, void *data);
static const ShellCommandItem vl_als_set_iperiod_command_items[] = {
  {NULL, param_handler, NULL},
  {NULL, NULL, vl_als_set_iperiod_handler}
};
static const ShellCommand vl_als_set_iperiod_command = {
  vl_als_set_iperiod_command_items,
  "vl6_als_set_iperiod",
  "vl6_als_set_iperiod iperiod",
  NULL,
  NULL
};

static int vl_als_get_handler(printf_func pfunc, gets_func gfunc, int argc, char **argv, void *data)
{
  float coefficient = atof(argv[0]);
  unsigned short result;
  unsigned int rc = vl6180_getAlsValue(&result);
  if (rc)
    return rc;
  float luminocity = (float)result * coefficient;
  pfunc("luminocity: %f\n", luminocity);
  return 0;
}

static int vl_init_handler(printf_func pfunc, gets_func gfunc, int argc, char **argv, void *data)
{
  return vl6180_init(&cfg);
}

static int vl_interrupt_clear_handler(printf_func pfunc, gets_func gfunc, int argc, char **argv, void *data)
{
  return vl6180_interruptClear(7);
}

static int vl_als_start_handler(printf_func pfunc, gets_func gfunc, int argc, char **argv, void *data)
{
  int flags = atoi(argv[0]);
  if (flags < 0 || flags > 3)
  {
    pfunc("invalid flags\r\n");
    return 1;
  }
  return vl6180_sysAlsStart(flags);
}

static int vl_als_set_gain_handler(printf_func pfunc, gets_func gfunc, int argc, char **argv, void *data)
{
  int gain = atoi(argv[0]);
  if (gain < 0 || gain > 7)
  {
    pfunc("invalid gain\r\n");
    return 1;
  }
  return vl6180_sysAlsSetAnalogGain(gain);
}

static int vl_als_set_iperiod_handler(printf_func pfunc, gets_func gfunc, int argc, char **argv, void *data)
{
  int period = atoi(argv[0]);
  if (period < 0 || period > 255)
  {
    pfunc("invalid period\r\n");
    return 1;
  }
  return vl6180_sysAlsSetIntegrationPeriod(period);
}

static int vl_als_status_handler(printf_func pfunc, gets_func gfunc, int argc, char **argv, void *data)
{
  unsigned char status;
  int rc = vl6180_getAlsStatus(&status);
  if (rc)
  {
    pfunc("get als status failed\r\n");
    return rc;
  }
  pfunc("als status: 0x%02X\r\n", status);
  rc = vl6180ReadRegister(VL6180_REG_SYSALS_START, &status, 1);
  if (rc)
  {
    pfunc("get als start status failed\r\n");
    return rc;
  }
  pfunc("als start status: 0x%02X\r\n", status);
  return 0;
}

void register_vl6_commands(void)
{
  shell_register_command(&vl_init_command);
  shell_register_command(&vl_als_start_command);
  shell_register_command(&vl_als_status_command);
  shell_register_command(&vl_als_get_command);
  shell_register_command(&vl_interrupt_clear_command);
  shell_register_command(&vl_als_set_gain_command);
  shell_register_command(&vl_als_set_iperiod_command);
}

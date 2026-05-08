#include <freertos/FreeRTOS.h>
#include <freertos/task.h>
#include "ds3231_commands.h"
#include <shell.h>
#include <rtc_ds3231.h>

static int init_handler(printf_func pfunc, gets_func gfunc, int argc, char **argv, void *data);
static const ShellCommandItem init_command_items[] = {
  {nullptr, nullptr, init_handler}
};
static const ShellCommand init_command = {
  init_command_items,
  "ds3231_init",
  "ds3231_init",
  nullptr,
  nullptr
};

static int date_get_handler(printf_func pfunc, gets_func gfunc, int argc, char **argv, void *data);
static const ShellCommandItem date_get_command_items[] = {
  {nullptr, nullptr, date_get_handler}
};
static const ShellCommand date_get_command = {
  date_get_command_items,
  "ds3231_date_get",
  "ds3231_date_get",
  nullptr,
  nullptr
};

static int date_set_handler(printf_func pfunc, gets_func gfunc, int argc, char **argv, void *data);
static const ShellCommandItem date_set_command_items[] = {
  {NULL, param_handler, NULL}, // year
  {NULL, param_handler, NULL}, // month
  {NULL, param_handler, NULL}, // day
  {NULL, param_handler, NULL}, // hour
  {NULL, param_handler, NULL}, // minute
  {NULL, param_handler, NULL}, // second
  {NULL, NULL, date_set_handler}
};
static const ShellCommand date_set_command = {
  date_set_command_items,
  "ds3231_date_set",
  "ds3231_date_set year month day hour minute second",
  NULL,
  NULL
};

static int init_handler(printf_func pfunc, gets_func gfunc, int argc, char **argv, void *data)
{
  return ds3231_init(1);
}

static int date_get_handler(printf_func pfunc, gets_func gfunc, int argc, char **argv, void *data)
{
  rtc_data rdata;
  int rc = ds3231_get(&rdata);
  if (rc != 0)
    return rc;
  pfunc("Date: %d.%02d.%04d %02d:%02d:%02d weekday=%d yday=%d\n", rdata.day, rdata.month, rdata.year, rdata.hours,
        rdata.minutes, rdata.seconds, rdata.wday, rdata.yday);
  return 0;
}

static int date_set_handler(printf_func pfunc, gets_func gfunc, int argc, char **argv, void *data)
{
  int year = atoi(argv[0]);
  int month = atoi(argv[1]);
  int day = atoi(argv[2]);
  int hour = atoi(argv[3]);
  int minute = atoi(argv[4]);
  int second = atoi(argv[5]);
  if (year < 2026)
  {
    pfunc("Invalid year\r\n");
    return 1;
  }
  if (month < 1 || month > 12)
  {
    pfunc("Invalid month\r\n");
    return 1;
  }
  if (day < 1 || day > 31)
  {
    pfunc("Invalid day\r\n");
    return 1;
  }
  if (hour < 0 || hour > 23)
  {
    pfunc("Invalid hour\r\n");
    return 1;
  }
  if (minute < 0 || minute > 59)
  {
    pfunc("Invalid minute\r\n");
    return 1;
  }
  if (second < 0 || second > 59)
  {
    pfunc("Invalid second\r\n");
    return 1;
  }
  return ds3231_set_datetime(year, month, day, hour, minute, second);
}

void register_ds3231_commands(void)
{
  shell_register_command(&init_command);
  shell_register_command(&date_get_command);
  shell_register_command(&date_set_command);
}

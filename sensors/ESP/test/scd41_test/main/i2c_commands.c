#include <freertos/FreeRTOS.h>
#include "board.h"
#include <i2c_commands.h>
#include <shell.h>
#include "esp_err.h"
#include "esp_log_level.h"
#include "driver/i2c_master.h"

static int scan_handler(printf_func pfunc, gets_func gfunc, int argc, char **argv, void *data);
static const ShellCommandItem scan_command_items[] = {
  {nullptr, nullptr, scan_handler}
};
static const ShellCommand scan_command = {
  scan_command_items,
  "i2c_scan",
  "i2c_scan",
  nullptr,
  nullptr
};

static int scan_handler(printf_func pfunc, gets_func gfunc, int argc, char **argv, void *data)
{
  esp_log_level_set("i2c.master", ESP_LOG_NONE);
  pfunc("     0  1  2  3  4  5  6  7  8  9  a  b  c  d  e  f\n");
  pfunc("00:   ");

  for (uint16_t i = 1; i <= 0x7F; i++)
  {
    esp_err_t res = i2c_probe(i, 10);
    if (i % 16 == 0)
      pfunc("\n%.2x:", i);
    if (res == 0)
      pfunc(" %.2x", i);
    else
      pfunc(" --");
    vTaskDelay(pdMS_TO_TICKS(1));
  }
  esp_log_level_set("i2c.master", ESP_LOG_INFO);

  return 0;
}

void register_i2c_commands(void)
{
  shell_register_command(&scan_command);
}

#include "board.h"
#include <i2c_commands.h>
#include <shell.h>

#include "esp_err.h"
#include "driver/i2c.h"

static int scan_handler(printf_func pfunc, gets_func gfunc, int argc, char **argv, void *data);
static const ShellCommandItem scan_command_items[] = {
  {NULL, NULL, scan_handler}
};
static const ShellCommand scan_command = {
  scan_command_items,
  "i2c_scan",
  "i2c_scan",
  NULL,
  NULL
};

static int scan_handler(printf_func pfunc, gets_func gfunc, int argc, char **argv, void *data)
{
  esp_err_t res;
  pfunc("     0  1  2  3  4  5  6  7  8  9  a  b  c  d  e  f\n");
  pfunc("00:   ");

  for (uint8_t i = 1; i <= 0x7F; i++)
  {
    i2c_cmd_handle_t cmd = i2c_cmd_link_create();
    i2c_master_start(cmd);
    i2c_master_write_byte(cmd, (i << 1) | I2C_MASTER_WRITE, 1 /* expect ack */);
    i2c_master_stop(cmd);

    res = i2c_master_cmd_begin(I2C_NUM_0, cmd, 10 / portTICK_PERIOD_MS);
    if (i % 16 == 0)
      pfunc("\n%.2x:", i);
    if (res == 0)
      pfunc(" %.2x", i);
    else
      pfunc(" --");
    i2c_cmd_link_delete(cmd);
    vTaskDelay(pdMS_TO_TICKS(1));
  }

  return 0;
}

void register_i2c_commands(void)
{
  shell_register_command(&scan_command);
}

#include <freertos/FreeRTOS.h>
#include <freertos/task.h>
#include "scd41_commands.h"
#include <shell.h>
#include <scd4x.h>

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
  scd4x_result result;
  scd4x_wake_up();
  esp_err_t rc = scd4x_start_measurement();
  if (rc != ESP_OK)
  {
    pfunc("scd4x_start_measurement failed\n");
    return rc;
  }
  vTaskDelay(6000 / portTICK_PERIOD_MS);
  rc = scd4x_read_measurement(&result);
  if (rc != ESP_OK)
  {
    pfunc("scd4x_read_measurement failed\n");
    return rc;
  }
  rc = scd4x_power_down();
  if (rc != ESP_OK)
  {
    pfunc("scd4x_power_down failed\n");
    return rc;
  }
  pfunc("CO2: %d\ntemperature: %d\nhumidity: %d\n", result.co2, result.temperature, result.humidity);
  return 0;
}

void register_scd41_commands(void)
{
  shell_register_command(&scd_get_command);
}

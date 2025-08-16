#include "freertos/FreeRTOS.h"
#include "board.h"
#include "driver/uart.h"
#include <shell.h>
#include <getstring.h>
#include <string.h>
#include "wifi_commands.h"
#include <wifi.h>
#include "sht_commands.h"
#include "scd_commands.h"
#include "display_commands.h"
#include "epd_ssd1680.h"
#include "i2c_commands.h"
#include "ui_commands.h"
#include "cc1101_commands.h"
#include "bh_commands.h"
#include "vl50_commands.h"
#include "vl51_commands.h"
#include "vl6_commands.h"
#include "esp_log.h"
#include "nvs_flash.h"

static const char *TAG = "main";

static char command_line[200];

static int getch_(void)
{
  int c = getchar();
  if (c == '\n')
    c = '\r';
  return c;
}

static char *gets_(void)
{
  getstring_next();
  return command_line;
}

static void puts_(const char *s)
{
  while (*s)
    putchar(*s++);
}

static void nvs_init(void)
{
  int rc = nvs_flash_init();
  if (rc == ESP_ERR_NVS_NO_FREE_PAGES || rc == ESP_ERR_NVS_NEW_VERSION_FOUND) {
    // NVS partition was truncated and needs to be erased
    // Retry nvs_flash_init
    nvs_flash_erase();
    rc = nvs_flash_init();
  }
  if (rc != 0)
  {
    ESP_LOGE(TAG, "nvs_flash_init failed");
    //set_led_red();
    while (1){}
  }
}

void app_main(void)
{
  int rc;

  configure_hal();

  shell_init(printf, gets_);
  register_wifi_commands();
  register_sht_commands();
  register_scd_commands();
  register_display_commands();
  register_i2c_commands();
  register_ui_commands();
  register_cc1101_commands();
  register_bh_commands();
  register_vl6_commands();
  register_vl50_commands();
  register_vl51_commands();

  getstring_init(command_line, sizeof(command_line), getch_, puts_);

  unsigned long start_time = xTaskGetTickCount();

  nvs_init();
  wifi_init();

  ssd1680_init();

  for (;;)
  {
    if (!getstring_next())
    {
      switch (command_line[0])
      {
        case SHELL_UP_KEY:
          puts_("\r\33[2K$ ");
        getstring_buffer_init(shell_get_prev_from_history());
        continue;
        case SHELL_DOWN_KEY:
          puts_("\r\33[2K$ ");
        getstring_buffer_init(shell_get_next_from_history());
        continue;
        default:
          rc = shell_execute(command_line);
        if (rc == 0)
          puts_("OK\r\n$ ");
        else if (rc < 0)
          puts_("Invalid command line\r\n$ ");
        else
          printf("shell_execute returned %d\n$ ", rc);
        break;
      }
    }
    unsigned long new_time = xTaskGetTickCount();
    if (new_time - start_time >= 1000 / portTICK_PERIOD_MS)
    {
      blink_led();
      start_time = new_time;
    }
    vTaskDelay(100 / portTICK_PERIOD_MS);
  }
}

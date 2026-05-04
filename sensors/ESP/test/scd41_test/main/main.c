#include <esp_log.h>
#include <freertos/FreeRTOS.h>
#include <freertos/task.h>
#include "led.h"
#include <getstring.h>
#include <shell.h>
#include "board.h"
#include "scd41_commands.h"

#define LOG_TAG "main"

static char command_line[200];

static int getch_(void)
{
  int c = getchar();
  if (c == '\n')
    c = '\r';
  return c;
}

static char* gets_(void)
{
  getstring_next();
  return command_line;
}

static void puts_(const char* s)
{
  while (*s)
    putchar(*s++);
}

void app_main(void) {
  configure_led();

  esp_err_t err = i2c_master_init();
  if (err != ESP_OK)
  {
    ESP_LOGE(LOG_TAG, "i2c_master_init failed with error %d", err);
    set_led_red();
    while (true)
      vTaskDelay(1000 / portTICK_PERIOD_MS);
  }

  //set_led_white();

  shell_init(printf, gets_);
  register_scd41_commands();

  getstring_init(command_line, sizeof(command_line), getch_, puts_);

  int rc;

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
    vTaskDelay(100 / portTICK_PERIOD_MS);
  }
}

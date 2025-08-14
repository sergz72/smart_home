#include "custom.h"
#include <stdio.h>
#include <getstring.h>
#include <shell.h>
#include <epd_ssd1680.h>
#include "ui.h"
#include "wifi_commands.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "env.h"

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

void vTaskCode(void * pvParameters)
{
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

void custom_init(void)
{
  shell_init(printf, gets_);
  register_wifi_commands();

  getstring_init(command_line, sizeof(command_line), getch_, puts_);

  ssd1680_init();

  UI_Init();

  xTaskCreate(vTaskCode,       /* Function that implements the task. */
       "shell task",    /* Text name for the task. */
              2048,      /* Stack size in words, not bytes. */
              NULL,    /* Parameter passed into the task. */
              tskIDLE_PRIORITY,/* Priority at which the task is created. */
              NULL);      /* Used to pass out the created task's handle. */
}

void custom_update(void)
{
  UI_Update();
}

#include <shell.h>
#include "wifi_commands.h"
#include <string.h>
#include <wifi.h>
#include <nvs.h>

static int set_handler(printf_func pfunc, gets_func gfunc, int argc, char **argv, void *data);
static const ShellCommandItem set_command_items[] = {
  {NULL, param_handler, NULL},
  {NULL, param_handler, NULL},
  {NULL, NULL, set_handler}
};
static const ShellCommand set_command = {
  set_command_items,
  "wifi_set",
  "wifi_set ssid password",
  NULL,
  NULL
};

static int reboot_handler(printf_func pfunc, gets_func gfunc, int argc, char **argv, void *data);
static const ShellCommandItem reboot_command_items[] = {
  {NULL, NULL, reboot_handler}
};
static const ShellCommand reboot_command = {
  reboot_command_items,
  "reboot",
  "reboot",
  NULL,
  NULL
};

static int set_handler(printf_func pfunc, gets_func gfunc, int argc, char **argv, void *data)
{
  strcpy((char*)wifi_config.sta.ssid, argv[0]);
  strcpy((char*)wifi_config.sta.password, argv[1]);
  printf("%s\n%s\n", argv[0], argv[1]);
  nvs_set_wifi_name();
  nvs_set_wifi_pwd();
  return 0;
}

static int reboot_handler(printf_func pfunc, gets_func gfunc, int argc, char **argv, void *data)
{
  esp_restart();
}

void register_wifi_commands(void)
{
  shell_register_command(&set_command);
  shell_register_command(&reboot_command);
}

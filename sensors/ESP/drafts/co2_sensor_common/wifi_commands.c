#include <shell.h>
#include "wifi_commands.h"
#include <string.h>
#include <wifi.h>

static int connect_handler(printf_func pfunc, gets_func gfunc, int argc, char **argv, void *data);
static const ShellCommandItem connect_command_items[] = {
  {NULL, param_handler, NULL},
  {NULL, param_handler, NULL},
  {NULL, NULL, connect_handler}
};
static const ShellCommand connect_command = {
  connect_command_items,
  "wifi_connect",
  "wifi_connect ssid password",
  NULL,
  NULL
};

static int disconnect_handler(printf_func pfunc, gets_func gfunc, int argc, char **argv, void *data);
static const ShellCommandItem disconnect_command_items[] = {
  {NULL, NULL, disconnect_handler}
};
static const ShellCommand disconnect_command = {
  disconnect_command_items,
  "wifi_disconnect",
  "wifi_disconnect",
  NULL,
  NULL
};

static int connect_handler(printf_func pfunc, gets_func gfunc, int argc, char **argv, void *data)
{
  strcpy((char*)wifi_config.sta.ssid, argv[0]);
  strcpy((char*)wifi_config.sta.password, argv[1]);
  printf("%s\n%s\n", argv[0], argv[1]);
  return wifi_connect();
}

static int disconnect_handler(printf_func pfunc, gets_func gfunc, int argc, char **argv, void *data)
{
  return esp_wifi_stop();
}

void register_wifi_commands(void)
{
  shell_register_command(&connect_command);
  shell_register_command(&disconnect_command);
}

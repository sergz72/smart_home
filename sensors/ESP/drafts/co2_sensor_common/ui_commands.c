#include "board.h"
#include <ui_commands.h>
#include <shell.h>
#include <epd_ssd1680.h>
#include <read_hex_string.h>

static int ui_refresh_handler(printf_func pfunc, gets_func gfunc, int argc, char **argv, void *data);
static const ShellCommandItem ui_refresh_command_items[] = {
  {NULL, NULL, ui_refresh_handler}
};
static const ShellCommand ui_refresh_command = {
  ui_refresh_command_items,
  "ui_refresh",
  "ui_refresh",
  NULL,
  NULL
};

static int fill_screen_handler(printf_func pfunc, gets_func gfunc, int argc, char **argv, void *data);
static const ShellCommandItem fill_screen_command_items[] = {
  {NULL, param_handler, NULL},
  {NULL, NULL, fill_screen_handler}
};
static const ShellCommand fill_screen_command = {
  fill_screen_command_items,
  "fill_screen",
  "fill_screen color",
  NULL,
  NULL
};

static int fill_rect_handler(printf_func pfunc, gets_func gfunc, int argc, char **argv, void *data);
static const ShellCommandItem fill_rect_command_items[] = {
  {NULL, param_handler, NULL},
  {NULL, param_handler, NULL},
  {NULL, param_handler, NULL},
  {NULL, param_handler, NULL},
  {NULL, param_handler, NULL},
  {NULL, NULL, fill_rect_handler}
};
static const ShellCommand fill_rect_command = {
  fill_rect_command_items,
  "fill_rect",
  "fill_rect x1 dx y1 dy color",
  NULL,
  NULL
};

static int ui_refresh_handler(printf_func pfunc, gets_func gfunc, int argc, char **argv, void *data)
{
  ssd1680_refresh();
  return 0;
}

static int fill_screen_handler(printf_func pfunc, gets_func gfunc, int argc, char **argv, void *data)
{
  int color = atoi(argv[0]);
  if (color < 0 || color > 2)
  {
    pfunc("Invalid color\r\n");
    return 1;
  }
  ssd1680_fill_screen(color);
  return 0;
}

static int fill_rect_handler(printf_func pfunc, gets_func gfunc, int argc, char **argv, void *data)
{
  int x1 = atoi(argv[0]);
  int dx = atoi(argv[1]);
  int y1 = atoi(argv[2]);
  int dy = atoi(argv[3]);
  int color = atoi(argv[4]);
  if (color < 0 || color > 2)
  {
    pfunc("Invalid color\r\n");
    return 1;
  }
  if (x1 < 0 || x1 >= LCD_WIDTH)
  {
    pfunc("Invalid x1\r\n");
    return 1;
  }
  if (y1 < 0 || y1 >= LCD_HEIGHT)
  {
    pfunc("Invalid y1\r\n");
    return 1;
  }
  if (dx < 0 || dx > LCD_WIDTH - x1)
  {
    pfunc("Invalid dx\r\n");
    return 1;
  }
  if (dy < 0 || dy > LCD_HEIGHT - y1)
  {
    pfunc("Invalid dy\r\n");
    return 1;
  }
  ssd1680_fill_rect((unsigned short)x1, (unsigned short)dx, (unsigned short)y1, (unsigned short)dy, color);
  return 0;
}

void register_ui_commands(void)
{
  shell_register_command(&fill_screen_command);
  shell_register_command(&fill_rect_command);
  shell_register_command(&ui_refresh_command);
}

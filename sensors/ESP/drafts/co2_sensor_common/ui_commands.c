#include "board.h"
#include <ui_commands.h>
#include <shell.h>
#include <epd_ssd1680.h>
#include <fonts/font16.h>
#include <fonts/font36_2.h>
#include <ui.h>

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

static int ui_init_handler(printf_func pfunc, gets_func gfunc, int argc, char **argv, void *data);
static const ShellCommandItem ui_init_command_items[] = {
  {NULL, NULL, ui_init_handler}
};
static const ShellCommand ui_init_command = {
  ui_init_command_items,
  "ui_init",
  "ui_init",
  NULL,
  NULL
};

static int ui_update_handler(printf_func pfunc, gets_func gfunc, int argc, char **argv, void *data);
static const ShellCommandItem ui_update_command_items[] = {
  {NULL, NULL, ui_update_handler}
};
static const ShellCommand ui_update_command = {
  ui_update_command_items,
  "ui_update",
  "ui_update",
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
  {NULL, param_handler, NULL}, // x1
  {NULL, param_handler, NULL}, // dx
  {NULL, param_handler, NULL}, // y1
  {NULL, param_handler, NULL}, // dy
  {NULL, param_handler, NULL}, // color
  {NULL, NULL, fill_rect_handler}
};
static const ShellCommand fill_rect_command = {
  fill_rect_command_items,
  "fill_rect",
  "fill_rect x1 dx y1 dy color",
  NULL,
  NULL
};

static int draw_char_handler(printf_func pfunc, gets_func gfunc, int argc, char **argv, void *data);
static const ShellCommandItem draw_char_command_items[] = {
  {NULL, param_handler, NULL}, // font size
  {NULL, param_handler, NULL}, // x
  {NULL, param_handler, NULL}, // y
  {NULL, param_handler, NULL}, // char
  {NULL, param_handler, NULL}, // textColor
  {NULL, param_handler, NULL}, // bkColor
  {NULL, NULL, draw_char_handler}
};
static const ShellCommand draw_char_command = {
  draw_char_command_items,
  "draw_char",
  "draw_char font_size x y char textColor bkColor",
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

static int draw_char_handler(printf_func pfunc, gets_func gfunc, int argc, char **argv, void *data)
{
  int font_size = atoi(argv[0]);
  const FONT_INFO *font;
  if (font_size == 16)
    font = &courierNew16ptFontInfo;
  else if (font_size == 36)
    font = &courierNew36ptFontInfo;
  else
  {
    pfunc("Invalid font size\r\n");
    return 1;
  }
  int x = atoi(argv[1]);
  int y = atoi(argv[2]);
  int textColor = atoi(argv[4]);
  if (textColor < 0 || textColor > 2)
  {
    pfunc("Invalid text color\r\n");
    return 1;
  }
  int bkColor = atoi(argv[5]);
  if (bkColor < 0 || bkColor > 2)
  {
    pfunc("Invalid background color\r\n");
    return 1;
  }
  if (x < 0 || x >= LCD_WIDTH)
  {
    pfunc("Invalid x\r\n");
    return 1;
  }
  if (y < 0 || y >= LCD_HEIGHT)
  {
    pfunc("Invalid y\r\n");
    return 1;
  }
  //pfunc("x: %d, y: %d, char: %c textColor %d bkColor %d\r\n", x, y, argv[3][0], textColor, bkColor);
  ssd1680_draw_char(x, y, argv[3][0], font, textColor, bkColor);
  return 0;
}

static int ui_init_handler(printf_func pfunc, gets_func gfunc, int argc, char **argv, void *data)
{
  UI_Init();
  return 0;
}

static int ui_update_handler(printf_func pfunc, gets_func gfunc, int argc, char **argv, void *data)
{
  scd30_result result;
  unsigned int rc = scd30_measure(&result);
  if (rc)
    return (int)rc;
  UI_Update(&result);
  return 0;
}

void register_ui_commands(void)
{
  shell_register_command(&fill_screen_command);
  shell_register_command(&fill_rect_command);
  shell_register_command(&ui_refresh_command);
  shell_register_command(&draw_char_command);
  shell_register_command(&ui_init_command);
  shell_register_command(&ui_update_command);
}

#include "board.h"
#include <ui.h>
#include <display.h>
#include <fonts/font16.h>
#include <fonts/font36_2.h>
#include <epd_ssd1680.h>
#include <limits.h>
#include <env.h>

static unsigned int prev_temperature, prev_humidity, prev_co2;
static int ui_off;

void UI_Init(void)
{
  Character c;

  ui_off = 0;

  prev_temperature = INT_MAX;
  prev_humidity = INT_MAX;
  prev_co2 = INT_MAX;

  ssd1680_fill_screen(ColorWhite);

  DisplayInit();

  c.font = &courierNew36ptFontInfo;
  c.y = 0;
  c.textColor = ColorBlack;
  c.bkColor = ColorWhite;
  for (unsigned int i = 0; i < DISPLAY_MAX_ROWS; i++)
  {
    c.x = 0;
    DisplayInitChar(0, i, &c);
    c.x += 40;
    c.font = &courierNew16ptFontInfo;
    DisplayInitChar(1, i, &c);
    c.y += 20;
    DisplayInitChar(2, i, &c);
    c.y -= 20;
    c.font = &courierNew36ptFontInfo;
    DisplayInitChar(3, i, &c);
    c.x += 40;
    DisplayInitChar(4, i, &c);
    c.x += 40;
    DisplayInitChar(5, i, &c);
    c.x += 40;
    DisplayInitChar(6, i, &c);
    c.x += 40;
    DisplayInitChar(7, i, &c);
    c.y += 40;
  }
  DisplaySetChar(0, 0, 't');
  DisplaySetChar(5, 0, '.');
  DisplaySetChar(7, 0, 'C');
  DisplaySetChar(0, 1, 'h');
  DisplaySetChar(5, 1, '.');
  DisplaySetChar(7, 1, '%');
  DisplaySetChar(0, 2, 'C');
  DisplaySetChar(1, 2, 'o');
  DisplaySetChar(2, 2, '2');
}

static void DisplayValue2p1(int row, unsigned int ivalue, int min, int max)
{
  unsigned short textColor = ivalue < min || ivalue > max ? ColorRed : ColorBlack;
  DisplaySetCharWithColor(3, row, (char)(ivalue >= 100 ? ivalue / 100 + '0' : ' '), textColor, ColorWhite);
  DisplaySetCharWithColor(4, row, (char)(((ivalue / 10) % 10) + '0'), textColor, ColorWhite);
  DisplaySetCharWithColor(6, row, (char)((ivalue % 10) + '0'), textColor, ColorWhite);
}

static void DisplayValue4(unsigned int ivalue)
{
  unsigned short textColor = ivalue > 1400 ? ColorRed : ColorBlack;
  DisplaySetCharWithColor(4, 2, (char)(ivalue >= 1000 ? ivalue / 1000 + '0' : ' '), textColor, ColorWhite);
  DisplaySetCharWithColor(5, 2, (char)(((ivalue / 100) % 10) + '0'), textColor, ColorWhite);
  DisplaySetCharWithColor(6, 2, (char)(((ivalue / 10) % 10) + '0'), textColor, ColorWhite);
  DisplaySetCharWithColor(7, 2, (char)((ivalue % 10) + '0'), textColor, ColorWhite);
}

void UI_Update(void)
{
  if (ui_off)
  {
    if (luminocity >= LUX_ON)
      UI_Init();
    else
      return;
  }
  if (luminocity < LUX_OFF)
  {
    ui_off = 1;
    ssd1680_fill_screen(ColorWhite);
    ssd1680_refresh();
    return;
  }

  int updated = 0;
  unsigned int value = (unsigned int)(result_scd30.temperature * 10);
  if (value != prev_temperature)
  {
    updated = 1;
    prev_temperature = value;
    DisplayValue2p1(0, value, 200, 280);
  }
  value = (unsigned int)(result_scd30.humidity * 10);
  if (value != prev_humidity)
  {
    updated = 1;
    prev_humidity = value;
    DisplayValue2p1(1, value, 400, 600);
  }
  value = (unsigned int)result_scd30.co2;
  if (value != prev_co2)
  {
    updated = 1;
    prev_co2 = value;
    DisplayValue4(value);
  }
  if (updated)
    ssd1680_refresh();
}

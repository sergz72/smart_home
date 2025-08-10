#include "board.h"
#include <ui.h>
#include <display.h>
#include <fonts/font16.h>
#include <fonts/font36_2.h>
#include <epd_ssd1680.h>

void UI_Init(void)
{
  Character c;

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
  ssd1680_refresh();
}

static void DisplayValue2p1(int row, float value, int min, int max)
{
  unsigned int ivalue = (unsigned int)(value * 10);
  unsigned short textColor = ivalue < min || ivalue > max ? ColorRed : ColorBlack;
  DisplaySetCharWithColor(3, row, (char)(ivalue >= 100 ? ivalue / 100 + '0' : ' '), textColor, ColorWhite);
  DisplaySetCharWithColor(4, row, (char)(((ivalue / 10) % 10) + '0'), textColor, ColorWhite);
  DisplaySetCharWithColor(6, row, (char)((ivalue % 10) + '0'), textColor, ColorWhite);
}

static void DisplayValue4(float value)
{
  unsigned int ivalue = (unsigned int)value;
  unsigned short textColor = ivalue > 1400 ? ColorRed : ColorBlack;
  DisplaySetCharWithColor(4, 2, (char)(ivalue >= 1000 ? ivalue / 1000 + '0' : ' '), textColor, ColorWhite);
  DisplaySetCharWithColor(5, 2, (char)(((ivalue / 100) % 10) + '0'), textColor, ColorWhite);
  DisplaySetCharWithColor(6, 2, (char)(((ivalue / 10) % 10) + '0'), textColor, ColorWhite);
  DisplaySetCharWithColor(7, 2, (char)((ivalue % 10) + '0'), textColor, ColorWhite);
}

void UI_Update(const scd30_result *result)
{
  DisplayValue2p1(0, result->temperature, 200, 280);
  DisplayValue2p1(1, result->humidity, 400, 600);
  DisplayValue4(result->co2);
  ssd1680_refresh();
}

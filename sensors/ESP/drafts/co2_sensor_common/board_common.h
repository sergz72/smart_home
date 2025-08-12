#ifndef _BOARD_COMMON_H
#define _BOARD_COMMON_H

#ifndef NULL
#define NULL 0
#endif

#define MAX_SHELL_COMMANDS                 50
#define MAX_SHELL_COMMAND_PARAMETERS       10
#define MAX_SHELL_COMMAND_PARAMETER_LENGTH 50
#define SHELL_HISTORY_SIZE                 20
#define SHELL_HISTORY_ITEM_LENGTH          100

#define I2C_MASTER_FREQ_HZ    50000
#define I2C_MASTER_NUM        0
#define I2C_MASTER_TIMEOUT_MS 1000

#define LCD_WIDTH 250
#define LCD_HEIGHT 122
#define LCD_SWAPXY

#include <driver/gpio.h>

#define SSD1680_RES_CLR gpio_set_level(PIN_RES, 0)
#define SSD1680_RES_SET gpio_set_level(PIN_RES, 1)
#define SSD1680_GET_BUSY gpio_get_level(PIN_BUSY)
#define SSD1680_DC_CLR gpio_set_level(PIN_DC, 0)
#define SSD1680_DC_SET gpio_set_level(PIN_DC, 1)
#define SSD1680_CS_CLR gpio_set_level(PIN_DISPLAY_CS, 0)
#define SSD1680_CS_SET gpio_set_level(PIN_DISPLAY_CS, 1)

#define SSD1680_DATA_ENTRY_MODE DATA_ENTRY_DECRY_INCRX_YUPDATE

#define DISPLAY_MAX_ROWS 3
#define DISPLAY_MAX_COLUMNS 8
#define DISPLAY_MAX_RECTANGLES 0

#define CC1101_TIMEOUT 0xFFFFF

#ifdef PIN_GDO0
#define cc1101_GD2 gpio_get_level(PIN_GDO2)
#define cc1101_GD0 gpio_get_level(PIN_GDO0)
#else
#define cc1101_GD2 0
#define cc1101_GD0 0
#endif

#define delay(x) delayms(1)
#define cc1101_CSN_CLR(x) gpio_set_level(PIN_CC1101_CS, 0)
#define cc1101_CSN_SET(x) gpio_set_level(PIN_CC1101_CS, 1)

#define BH1750_ADDR 0x23

#ifdef PIN_VL6180_IO0
#define VL6180_IO0_HIGH gpio_set_level(PIN_VL6180_IO0, 1)
#define VL6180_IO0_LOW gpio_set_level(PIN_VL6180_IO0, 0)
#else
#define VL6180_IO0_HIGH
#define VL6180_IO0_LOW
#endif

void configure_hal(void);
void blink_led(void);
void delayms(unsigned int ms);
void Log(const char *name, int value);

#endif

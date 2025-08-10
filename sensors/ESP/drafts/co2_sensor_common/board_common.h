#ifndef _BOARD_COMMON_H
#define _BOARD_COMMON_H

#ifndef NULL
#define NULL 0
#endif

#define MAX_SHELL_COMMANDS                 30
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
#define SSD1680_CS_CLR gpio_set_level(PIN_CS, 0)
#define SSD1680_CS_SET gpio_set_level(PIN_CS, 1)

#define SSD1680_DATA_ENTRY_MODE DATA_ENTRY_DECRY_INCRX_YUPDATE

#define DISPLAY_MAX_ROWS 3
#define DISPLAY_MAX_COLUMNS 8
#define DISPLAY_MAX_RECTANGLES 0

void configure_hal(void);
void blink_led(void);
void delayms(unsigned int ms);
void Log(const char *name, int value);

#endif

#ifndef _BOARD_H
#define _BOARD_H

#define PIN_SCL 0
#define PIN_SDA 1

#define MAX_SHELL_COMMANDS                 50
#define MAX_SHELL_COMMAND_PARAMETERS       10
#define MAX_SHELL_COMMAND_PARAMETER_LENGTH 50
#define SHELL_HISTORY_SIZE                 20
#define SHELL_HISTORY_ITEM_LENGTH          100

#define I2C_MASTER_FREQ_HZ    50000
#define I2C_MASTER_NUM        0
#define I2C_MASTER_TIMEOUT_MS 1000

#include <esp_err.h>

esp_err_t i2c_master_init(void);

#endif

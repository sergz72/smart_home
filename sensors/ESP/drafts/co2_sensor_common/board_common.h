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

#define I2C_MASTER_FREQ_HZ    100000
#define I2C_MASTER_NUM        0
#define I2C_MASTER_TIMEOUT_MS 1000

void configure_hal(void);
void blink_led(void);

#endif

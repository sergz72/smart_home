#ifndef _SCD4X_H
#define _SCD4X_H

#define SCD4X_SENSOR_ADDR 0x62

#include "esp_err.h"

typedef struct
{
  unsigned short temperature;
  unsigned short humidity;
  unsigned int co2;
} scd4x_result;

esp_err_t scd4x_command(uint8_t *wdata, size_t wlen, uint8_t *rdata, size_t rlen); // should be defined in hal.c
esp_err_t scd4x_write(uint8_t *data, size_t len); // should be defined in hal.c
esp_err_t scd4x_read(uint8_t *data, size_t len); // should be defined in hal.c

esp_err_t scd4x_read_measurement (scd4x_result *result);
esp_err_t scd4x_start_measurement(void);
esp_err_t scd4x_power_down(void);
esp_err_t scd4x_wake_up(void);

#endif

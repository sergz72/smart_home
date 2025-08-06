#ifndef _SCD30_H
#define _SCD30_H

#define SCD30_SENSOR_ADDR 0x61

#include "esp_err.h"

typedef struct
{
  float temperature;
  float humidity;
  float co2;
} scd30_result;

esp_err_t scd30_command(uint8_t *wdata, size_t wlen, uint8_t *rdata, size_t rlen); // should be defined in hal.c
esp_err_t scd30_write(uint8_t *data, size_t len); // should be defined in hal.c
esp_err_t scd30_read(uint8_t *data, size_t len); // should be defined in hal.c

esp_err_t scd30_init_sensor(uint16_t interval);
esp_err_t scd30_measure (scd30_result *result);

#endif

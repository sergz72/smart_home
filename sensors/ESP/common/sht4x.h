#ifndef _SHT4X_H
#define _SHT4X_H

#include "esp_err.h"

esp_err_t sht40_register_read(uint8_t *rdata, size_t rlen, uint8_t *data, size_t len); // should be defined in hal.c
esp_err_t sht40_register_write(uint8_t *data, size_t len);
esp_err_t sht40_read(uint8_t *data, size_t len);

esp_err_t sht4x_init_sensor(void);
esp_err_t sht4x_measure (int16_t* temperature, uint16_t* humidity);

#endif

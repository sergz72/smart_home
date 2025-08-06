#ifndef TEMP_HUMI_PRES_SENSOR_SHT3X_H
#define TEMP_HUMI_PRES_SENSOR_SHT3X_H

#include "esp_err.h"

esp_err_t sht30_register_read(uint8_t *rdata, size_t rlen, uint8_t *data, size_t len);
esp_err_t sht30_register_write(uint8_t *data, size_t len);
esp_err_t sht30_read(uint8_t *data, size_t len);

esp_err_t sht3x_init_sensor(void);
esp_err_t sht3x_measure (int16_t* temperature, uint16_t* humidity);

#endif //TEMP_HUMI_PRES_SENSOR_SHT3X_H

#ifndef TEMP_HUMI_PRES_SENSOR_SHT3X_H
#define TEMP_HUMI_PRES_SENSOR_SHT3X_H

#include "esp_err.h"

esp_err_t sht3x_init_sensor(void);
esp_err_t sht3x_measure (int16_t* temperature, uint16_t* humidity);

#endif //TEMP_HUMI_PRES_SENSOR_SHT3X_H

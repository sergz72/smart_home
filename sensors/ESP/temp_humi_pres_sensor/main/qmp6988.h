#ifndef TEMP_HUMI_PRES_SENSOR_QMP6988_H
#define TEMP_HUMI_PRES_SENSOR_QMP6988_H

#include "esp_err.h"

esp_err_t qmp6988_init_sensor(void);
esp_err_t qmp6988_calcPressure(uint32_t *pressure, uint16_t *temperature);

#endif //TEMP_HUMI_PRES_SENSOR_QMP6988_H

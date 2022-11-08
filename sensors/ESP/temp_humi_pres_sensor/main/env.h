//
// Created by sergi on 04.09.2022.
//

#ifndef TEMP_HUMI_PRES_SENSOR_ENV_H
#define TEMP_HUMI_PRES_SENSOR_ENV_H

#include "esp_err.h"
#include "common.h"

extern int16_t temp_val;
extern uint16_t humi_val;
#ifdef SEND_PRESSURE
extern uint32_t pres_val;
#endif

int get_env(void);
void init_env(void);

esp_err_t sht30_register_read(uint8_t *rdata, size_t rlen, uint8_t *data, size_t len);
esp_err_t sht30_register_write(uint8_t *data, size_t len);
esp_err_t sht30_read(uint8_t *data, size_t len);

#ifdef SEND_PRESSURE
esp_err_t qmp6988_register_write(uint8_t *data, size_t len);
esp_err_t qmp6988_register_read(uint8_t *rdata, size_t rlen, uint8_t *data, size_t len);
#endif

#endif //TEMP_HUMI_PRES_SENSOR_ENV_H

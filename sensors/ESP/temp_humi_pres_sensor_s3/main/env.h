//
// Created by sergi on 04.09.2022.
//

#ifndef TEMP_HUMI_PRES_SENSOR_ENV_H
#define TEMP_HUMI_PRES_SENSOR_ENV_H

#include "esp_err.h"
#include "common.h"

extern int16_t temp_val;
extern uint16_t humi_val;
extern uint32_t pres_val;
extern int16_t ext_temp_val;
extern uint16_t ext_humi_val;
extern int16_t ext_temp_val2;
extern int16_t ext_temp_val3;
extern uint16_t ext_humi_val3;
extern uint32_t co2_level;

int get_env(void);
void init_env(void);
void post_init_env(void);

#endif //TEMP_HUMI_PRES_SENSOR_ENV_H

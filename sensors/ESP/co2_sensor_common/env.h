#ifndef CO2_SENSOR_C6_ENV_H
#define CO2_SENSOR_C6_ENV_H

#include <stdint.h>
#ifdef USE_SCD4X
#include "scd4x.h"
#else
#include "scd30.h"
#endif

#define SHT40_SENSOR_ADDR 0x44

extern int16_t temp_val;
extern uint16_t humi_val;
extern int16_t ext_temp_val;
extern uint32_t co2_level;
extern uint32_t luminocity;

#ifdef USE_SCD4X
extern scd4x_result result_scd4x;
#else
extern scd30_result result_scd30;
#endif

int get_env(void);
void init_env(void);
void post_init_env(void);

#endif //CO2_SENSOR_C6_ENV_H
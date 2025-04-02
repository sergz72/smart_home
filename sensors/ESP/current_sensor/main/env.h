//
// Created by sergi on 04.09.2022.
//

#ifndef CURRENT_SENSOR_ENV_H
#define CURRENT_SENSOR_ENV_H

#include "esp_err.h"
#include "common.h"

extern uint16_t current_val;

int get_env(void);
void init_env(void);
void post_init_env(void);

#endif //CURRENT_SENSOR_ENV_H

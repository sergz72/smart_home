#ifndef _HAL_H
#define _HAL_H

#include <esp_err.h>

#ifdef __cplusplus
extern "C" {
#endif

esp_err_t i2c_master_init(void);
bool button_is_pressed(void);
void initialise_button(void);
int get_vbat(void);
esp_err_t set_time_from_rtc(void);

#ifdef __cplusplus
}
#endif

#endif

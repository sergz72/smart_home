#ifndef _HAL_H
#define _HAL_H

#include <esp_err.h>

#ifdef __cplusplus
extern "C" {
#endif

esp_err_t i2c_master_init(void);

#ifdef __cplusplus
}
#endif

#endif

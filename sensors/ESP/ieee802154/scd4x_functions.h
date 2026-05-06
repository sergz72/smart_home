#ifndef _SCD4X_FUNCTIONS_H
#define _SCD4X_FUNCTIONS_H

#include <esp_err.h>
#include <scd4x.h>

#ifdef __cplusplus
extern "C" {
#endif

esp_err_t scd_get(scd4x_result *result);
int scd_event_convert(const scd4x_result *result, uint8_t **output, int *output_size);

#ifdef __cplusplus
}
#endif

#endif

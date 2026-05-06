#include <freertos/FreeRTOS.h>
#include <freertos/task.h>
#include "scd4x_functions.h"
#include <events.h>
#include <string.h>
#include "esp_log.h"
#include "hal.h"

static const char *TAG = "scd4x_functions";

int scd_event_convert(const scd4x_result *result, uint8_t **output, int *output_size)
{
  uint8_t *o = malloc(20);
  if (o == NULL)
    return 1;
  int32_t value;
  o[0] = VALUE_TYPE_TEMP_INT;
  value = result->temperature;
  memcpy(o + 1, &value, 4);
  o[5] = VALUE_TYPE_HUMI_INT;
  value = result->humidity;
  memcpy(o + 6, &value, 4);
  o[10] = VALUE_TYPE_CO2;
  memcpy(o + 11, &result->co2, 4);
  o[15] = VALUE_TYPE_VBAT;
  value = get_vbat();
  memcpy(o + 16, &value, 4);
  *output = 0;
  *output_size = 20;
  return 0;
}

esp_err_t scd_get(scd4x_result *result)
{
  scd4x_wake_up();
  esp_err_t rc = scd4x_start_measurement();
  if (rc != ESP_OK)
  {
    ESP_LOGE(TAG, "scd4x_start_measurement failed\n");
    return rc;
  }
  vTaskDelay(6000 / portTICK_PERIOD_MS);
  rc = scd4x_read_measurement(result);
  if (rc != ESP_OK)
  {
    ESP_LOGE(TAG, "scd4x_read_measurement failed\n");
    return rc;
  }
  rc = scd4x_power_down();
  if (rc != ESP_OK)
    ESP_LOGE(TAG, "scd4x_power_down failed\n");
  ESP_LOGI(TAG, "CO2: %d temperature: %d humidity: %d", result->co2, result->temperature, result->humidity);
  return ESP_OK;
}

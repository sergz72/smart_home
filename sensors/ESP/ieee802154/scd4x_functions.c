#include <freertos/FreeRTOS.h>
#include <freertos/task.h>
#include "scd4x_functions.h"
#include <events.h>
#include <string.h>

#include "common_functions.h"
#include "esp_log.h"
#include "esp_sleep.h"
#include "hal.h"

static const char *TAG = "scd4x_functions";

uint8_t scd_event[16];

void scd_event_convert(const scd4x_result *result)
{
  int32_t value;
  value = result->temperature;
  memcpy(scd_event, &value, 3);
  scd_event[3] = VALUE_TYPE_TEMP_INT;
  value = result->humidity;
  memcpy(scd_event+4, &value, 3);
  scd_event[7] = VALUE_TYPE_HUMI_INT;
  memcpy(scd_event+8, &result->co2, 3);
  scd_event[11] = VALUE_TYPE_CO2;
  value = get_vbat();
  memcpy(scd_event + 12, &value, 3);
  scd_event[15] = VALUE_TYPE_VBAT;
}

esp_err_t scd_get(scd4x_result *result, bool sleep)
{
  scd4x_wake_up();
  esp_err_t rc = scd4x_start_measurement();
  if (rc != ESP_OK)
  {
    ESP_LOGE(TAG, "scd4x_start_measurement failed\n");
    return rc;
  }
  if (sleep)
  {
    register_timer_wakeup(6000000);
    esp_light_sleep_start();
  }
  else
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

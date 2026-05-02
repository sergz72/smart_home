#include "board.h"
#include <scd4x.h>
#include <crc8.h>
#include "esp_log.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"

#define SCD4x_RAW_DATA_SIZE 18

static const char *TAG = "scd4x";

static uint8_t scd4x_raw_data [SCD4x_RAW_DATA_SIZE];

static esp_err_t validate_raw_data_item (int offset, const char *name)
{
  if (crc8(scd4x_raw_data+offset,2) != scd4x_raw_data[offset+2])
  {
    ESP_LOGE(TAG, "CRC check for %s failed", name);
    return ESP_ERR_INVALID_CRC;
  }
  return ESP_OK;
}

static esp_err_t validate_raw_data (void)
{
  int rc = validate_raw_data_item(0, "CO2");
  if (rc != ESP_OK)
    return rc;

  rc = validate_raw_data_item(3, "temperature");
  if (rc != ESP_OK)
    return rc;

  return validate_raw_data_item(6, "humidity");
}

static esp_err_t scd4x_compute_values (scd4x_result *result)
{
  unsigned int co2 =
    (((unsigned int)scd4x_raw_data[0]) << 8) |
    (unsigned int)scd4x_raw_data[1];
  unsigned int temperature =
    (((unsigned int)scd4x_raw_data[3]) << 8) |
    (unsigned int)scd4x_raw_data[4];
  unsigned int humidity =
    (((unsigned int)scd4x_raw_data[6]) << 8) |
    (unsigned int)scd4x_raw_data[7];
  result->co2 = co2 * 100;
  result->temperature = (unsigned short)(-4500 + 17500 * temperature / 65535);
  result->humidity = (unsigned short)(10000 * humidity / 65535);
  return ESP_OK;
}

esp_err_t scd4x_start_measurement(void)
{
  unsigned char data[2];
  data[0] = 0x21;
  data[1] = 0x9D;
  int rc = scd4x_write(data, 2);
  if (rc != ESP_OK)
  {
    ESP_LOGE(TAG, "start measurements failed, code %d", rc);
    return rc;
  }
  return ESP_OK;
}

esp_err_t scd4x_power_down(void)
{
  unsigned char data[2];
  data[0] = 0x36;
  data[1] = 0xE0;
  int rc = scd4x_write(data, 2);
  if (rc != ESP_OK)
  {
    ESP_LOGE(TAG, "start measurements failed, code %d", rc);
    return rc;
  }
  return ESP_OK;
}

esp_err_t scd4x_wake_up(void)
{
  unsigned char data[2];
  data[0] = 0x36;
  data[1] = 0xF6;
  int rc = scd4x_write(data, 2);
  if (rc != ESP_OK)
  {
    ESP_LOGE(TAG, "start measurements failed, code %d", rc);
    return rc;
  }
  vTaskDelay (30 / portTICK_PERIOD_MS);
  return ESP_OK;
}

static esp_err_t scd4x_get_status(uint16_t *status)
{
  unsigned char data[2];
  data[0] = 0xE4;
  data[1] = 0xB8;
  int rc = scd4x_write(data, 2);
  if (rc != ESP_OK)
  {
    ESP_LOGE(TAG, "get status write failed, code %d", rc);
    return rc;
  }
  vTaskDelay (2 / portTICK_PERIOD_MS);
  rc = scd4x_read(scd4x_raw_data, 3);
  if (rc != ESP_OK)
  {
    ESP_LOGE(TAG, "get status read failed, code %d", rc);
    return rc;
  }
  rc = validate_raw_data_item(0, "status");
  if (rc != ESP_OK)
    return rc;
  *status = ((uint16_t)scd4x_raw_data[0] << 8) | (uint16_t)scd4x_raw_data[1];
  return ESP_OK;
}

esp_err_t scd4x_read_measurement (scd4x_result *result)
{
  uint16_t status;
  for (;;)
  {
    int rc = scd4x_get_status(&status);
    if (rc != ESP_OK)
      return rc;
    if ((status & 0x3FF) != 0)
      break;
    ESP_LOGI(TAG, "data is not ready, waiting 1 second...");
    vTaskDelay (1000 / portTICK_PERIOD_MS); // 10 sec
  }

  unsigned char data[2];
  data[0] = 0xEC;
  data[1] = 0x05;
  int rc = scd4x_write(data, 2);
  if (rc != ESP_OK)
  {
    ESP_LOGE(TAG, "read measurements write failed, code %d", rc);
    return rc;
  }
  vTaskDelay (2 / portTICK_PERIOD_MS);
  rc = scd4x_read(scd4x_raw_data, sizeof(scd4x_raw_data));
  if (rc != ESP_OK)
  {
    ESP_LOGE(TAG, "read measurements read failed, code %d", rc);
    return rc;
  }
  ESP_LOGI(TAG, "raw_data: %02X %02X %02X %02X %02X %02X %02X %02X %02X",
    scd4x_raw_data[0], scd4x_raw_data[1], scd4x_raw_data[2], scd4x_raw_data[3],
    scd4x_raw_data[4], scd4x_raw_data[5], scd4x_raw_data[6], scd4x_raw_data[7],
    scd4x_raw_data[8], scd4x_raw_data[9]);
  rc = validate_raw_data();
  if (rc != ESP_OK)
    return rc;
  return scd4x_compute_values(result);
}

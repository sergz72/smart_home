#include "board.h"
#include <scd30.h>
#include <crc8.h>
#include "esp_log.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"

#define SCD30_RAW_DATA_SIZE 18

static const char *TAG = "scd30";

static uint8_t scd30_raw_data [SCD30_RAW_DATA_SIZE];

static esp_err_t validate_raw_data_item (int offset, const char *name)
{
  if (crc8(scd30_raw_data+offset,2) != scd30_raw_data[offset+2])
  {
    ESP_LOGE(TAG, "CRC check for %s failed", name);
    return ESP_ERR_INVALID_CRC;
  }
  return ESP_OK;
}

static esp_err_t validate_raw_data (void)
{
  int rc = validate_raw_data_item(0, "CO2 M");
  if (rc != ESP_OK)
    return rc;
  rc = validate_raw_data_item(3, "CO2 L");
  if (rc != ESP_OK)
    return rc;

  rc = validate_raw_data_item(6, "temperature M");
  if (rc != ESP_OK)
    return rc;
  rc = validate_raw_data_item(9, "temperature L");
  if (rc != ESP_OK)
    return rc;

  rc = validate_raw_data_item(12, "humidity M");
  if (rc != ESP_OK)
    return rc;
  return validate_raw_data_item(15, "humidity L");
}

static esp_err_t scd30_compute_values (scd30_result *result)
{
  unsigned int co2 =
    (((unsigned int)scd30_raw_data[0]) << 24) |
    (((unsigned int)scd30_raw_data[1]) << 16) |
    (((unsigned int)scd30_raw_data[3]) << 8) |
    (unsigned int)scd30_raw_data[4];
  unsigned int temperature =
    (((unsigned int)scd30_raw_data[6]) << 24) |
    (((unsigned int)scd30_raw_data[7]) << 16) |
    (((unsigned int)scd30_raw_data[9]) << 8) |
    (unsigned int)scd30_raw_data[10];
  unsigned int humidity =
    (((unsigned int)scd30_raw_data[12]) << 24) |
    (((unsigned int)scd30_raw_data[13]) << 16) |
    (((unsigned int)scd30_raw_data[15]) << 8) |
    (unsigned int)scd30_raw_data[16];
  result->co2 = *(float*)&co2;
  result->temperature = *(float*)&temperature;
  result->humidity = *(float*)&humidity;
  return ESP_OK;
}

esp_err_t scd30_start_measurements(void)
{
  unsigned char data[5];
  data[0] = 0x00;
  data[1] = 0x10;
  data[2] = 0x00;
  data[3] = 0x00;
  data[4] = crc8(&data[2], 2);
  int rc = scd30_write(data, 5);
  if (rc != ESP_OK)
  {
    ESP_LOGE(TAG, "start measurements failed, code %d", rc);
    return rc;
  }
  return ESP_OK;
}

esp_err_t scd30_set_measurements_interval(uint16_t interval)
{
  if (interval < 2)
    interval = 2;
  else if (interval > 1800)
    interval = 1800;
  unsigned char data[5];
  data[0] = 0x46;
  data[1] = 0x00;
  data[2] = (unsigned char)(interval >> 8);
  data[3] = (unsigned char)interval;
  data[4] = crc8(&data[2], 2);
  int rc = scd30_write(data, 5);
  if (rc != ESP_OK)
  {
    ESP_LOGE(TAG, "set measurements interval failed, code %d", rc);
    return rc;
  }
  return ESP_OK;
}

esp_err_t scd30_init_sensor(uint16_t interval)
{
  unsigned char data[2];
  // soft reset
  data[0] = 0xD3;
  data[1] = 0x04;
  int rc = scd30_write(data, 2);
  if (rc != ESP_OK)
  {
    ESP_LOGE(TAG, "soft reset failed, code %d", rc);
    return rc;
  }
  vTaskDelay (100 / portTICK_PERIOD_MS);
  rc = scd30_set_measurements_interval(interval);
  if (rc != ESP_OK)
    return rc;
  vTaskDelay (100 / portTICK_PERIOD_MS);
  return scd30_start_measurements();
}

esp_err_t scd30_measure (scd30_result *result)
{
  unsigned char data[2];
  data[0] = 0x03;
  data[1] = 0x00;
  int rc = scd30_write(data, 2);
  if (rc != ESP_OK)
  {
    ESP_LOGE(TAG, "read measurements write failed, code %d", rc);
    return rc;
  }
  vTaskDelay (10 / portTICK_PERIOD_MS);
  rc = scd30_read(scd30_raw_data, sizeof(scd30_raw_data));
  if (rc != ESP_OK)
  {
    ESP_LOGE(TAG, "read measurements read failed, code %d", rc);
    return rc;
  }
  ESP_LOGI(TAG, "raw_data: %02X %02X %02X %02X %02X %02X %02X %02X %02X %02X %02X %02X %02X %02X %02X %02X %02X %02X",
    scd30_raw_data[0], scd30_raw_data[1], scd30_raw_data[2], scd30_raw_data[3],
    scd30_raw_data[4], scd30_raw_data[5], scd30_raw_data[6], scd30_raw_data[7],
    scd30_raw_data[8], scd30_raw_data[9], scd30_raw_data[10], scd30_raw_data[11],
    scd30_raw_data[12], scd30_raw_data[13], scd30_raw_data[14], scd30_raw_data[15],
    scd30_raw_data[16], scd30_raw_data[17]);
  rc = validate_raw_data();
  if (rc != ESP_OK)
    return rc;
  return scd30_compute_values(result);
}

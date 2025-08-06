#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "esp_log.h"
#include "sht4x.h"
#include "env.h"
#include <crc8.h>

#ifdef SHT40_SENSOR_ADDR

#define SHT4x_RESET_CMD                0x94
#define SHT4x_RAW_DATA_SIZE            6

#define MEASUREMENT_DURATION           10 // ms
#define MEASURE_CMD                    0xFD

typedef uint8_t sht4x_raw_data_t [SHT4x_RAW_DATA_SIZE];

static const char *TAG = "sht4x";

static esp_err_t sht4x_send_command(unsigned char cmd)
{
  ESP_LOGI(TAG, "send command %02x", cmd);

  return sht40_register_write(&cmd, 1);
}

static esp_err_t sht4x_reset(void)
{
  esp_err_t rc;

  ESP_LOGI(TAG, "soft-reset triggered");

  rc = sht4x_send_command(SHT4x_RESET_CMD);
  if (rc != ESP_OK)
  {
    ESP_LOGE(TAG, "reset command failure %d", rc);
    return rc;
  }

  // wait for small amount of time needed (according to datasheet 1ms)
  vTaskDelay (2 / portTICK_PERIOD_MS);

  return 0;
}

static esp_err_t sht4x_get_raw_data(sht4x_raw_data_t raw_data)
{
  esp_err_t rc;

  // read raw data
  rc = sht40_read(raw_data, sizeof(sht4x_raw_data_t));
  if (rc != ESP_OK)
  {
    ESP_LOGE(TAG, "read raw data failed");
    return rc;
  }

  // check temperature crc
  if (crc8(raw_data,2) != raw_data[2])
  {
    ESP_LOGE(TAG, "CRC check for temperature data failed");
    return ESP_ERR_INVALID_CRC;
  }

  // check humidity crc
  if (crc8(raw_data+3,2) != raw_data[5])
  {
    ESP_LOGE(TAG, "CRC check for humidity data failed");
    return ESP_ERR_INVALID_CRC;
  }

  return ESP_OK;
}

static esp_err_t sht4x_compute_values (const sht4x_raw_data_t raw_data, int16_t* temperature, uint16_t* humidity)
{
  *temperature = (int16_t)((((((int)raw_data[0]) << 8) + raw_data[1]) * 17500) / 65535 - 4500);

  *humidity = (uint16_t)((((((unsigned int)raw_data[3]) << 8) + raw_data[4]) * 12500) / 65535 - 600);

  return ESP_OK;
}

esp_err_t sht4x_measure (int16_t* temperature, uint16_t* humidity)
{
  esp_err_t rc;

  if (!temperature || !humidity) return ESP_ERR_INVALID_ARG;

  rc = sht4x_send_command(MEASURE_CMD);
  if (rc != ESP_OK)
  {
    ESP_LOGE(TAG, "could not send start measurement command");
    return rc;
  }

  vTaskDelay (MEASUREMENT_DURATION);

  sht4x_raw_data_t raw_data;

  rc = sht4x_get_raw_data (raw_data);
  if (rc != ESP_OK)
    return rc;

  return sht4x_compute_values (raw_data, temperature, humidity);
}

esp_err_t sht4x_init_sensor(void)
{
  return sht4x_reset();
}

#endif

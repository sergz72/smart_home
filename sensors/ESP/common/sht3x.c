#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "esp_log.h"
#include "sht3x.h"
#include "env.h"
#include <crc8.h>

#ifdef SHT30_SENSOR_ADDR

#define SHT3x_STATUS_CMD               0xF32D
#define SHT3x_RESET_CMD                0x30A2
#define SHT3x_RAW_DATA_SIZE            6

#define MEASUREMENT_DURATION           15 // ms
#define MEASURE_CMD                    0x2400

typedef uint8_t sht3x_raw_data_t [SHT3x_RAW_DATA_SIZE];

static const char *TAG = "sht3x";

static esp_err_t sht3x_send_command(uint16_t cmd)
{
  uint8_t data[2] = { cmd >> 8, cmd & 0xff };

  ESP_LOGI(TAG, "send command MSB=%02x LSB=%02x", data[0], data[1]);

  return sht30_register_write(data, 2);
}

static esp_err_t sht3x_read(uint16_t cmd, uint8_t *data, size_t len)
{
  uint8_t rdata[2] = { cmd >> 8, cmd & 0xff };

  ESP_LOGI(TAG, "register read MSB=%02x LSB=%02x", data[0], data[1]);

  return sht30_register_read(rdata, 2, data, len);
}

static esp_err_t sht3x_get_status (uint16_t* status)
{
  esp_err_t rc;
  uint8_t  data[3];

  if (!status) return ESP_ERR_INVALID_ARG;

  rc = sht3x_read(SHT3x_STATUS_CMD, data, 3);
  if (rc != ESP_OK)
  {
    ESP_LOGE(TAG, "sht3x status command failure %d", rc);
    return rc;
  }

  *status = data[0] << 8 | data[1];
  return ESP_OK;
}

static esp_err_t sht3x_reset(void)
{
  esp_err_t rc;

  ESP_LOGI(TAG, "soft-reset triggered");

  rc = sht3x_send_command(SHT3x_RESET_CMD);
  if (rc != ESP_OK)
  {
    ESP_LOGE(TAG, "sht3x reset command failure %d", rc);
    return rc;
  }

  // wait for small amount of time needed (according to datasheet 0.5ms)
  vTaskDelay (100 / portTICK_PERIOD_MS);

  uint16_t status;

  // check the status after reset
  return sht3x_get_status(&status);
}

static esp_err_t sht3x_get_raw_data(sht3x_raw_data_t raw_data)
{
  esp_err_t rc;

  // read raw data
  rc = sht30_read(raw_data, sizeof(sht3x_raw_data_t));
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

static esp_err_t sht3x_compute_values (const sht3x_raw_data_t raw_data, int16_t* temperature, uint16_t* humidity)
{
  *temperature = (int16_t)(((((((int)raw_data[0]) << 8) + raw_data[1]) * 17500) / 65535) - 4500);

  *humidity = (uint16_t)((((((unsigned int)raw_data[3]) << 8) + raw_data[4]) * 10000) / 65535);

  return ESP_OK;
}

esp_err_t sht3x_measure (int16_t* temperature, uint16_t* humidity)
{
  esp_err_t rc;

  if (!temperature || !humidity) return ESP_ERR_INVALID_ARG;

  rc = sht3x_send_command(MEASURE_CMD);
  if (rc != ESP_OK)
  {
    ESP_LOGE(TAG, "could not send start measurement command");
    return rc;
  }

  vTaskDelay (MEASUREMENT_DURATION);

  sht3x_raw_data_t raw_data;

  rc = sht3x_get_raw_data (raw_data);
  if (rc != ESP_OK)
    return rc;

  return sht3x_compute_values (raw_data, temperature, humidity);
}

esp_err_t sht3x_init_sensor(void)
{
  return sht3x_reset();
}

#endif

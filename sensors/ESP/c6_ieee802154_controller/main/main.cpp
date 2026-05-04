#include <Ieee802154.h>
#include <driver/gpio.h>
#include <esp_log.h>
#include <freertos/FreeRTOS.h>
#include <freertos/task.h>
#include <esp_coexist.h>
#include "led.h"
#include "wifi.h"
#include "common_functions.h"
#include "psa_crypto.h"
#include "hal.h"
#include "ntp.h"
#include "ieee802154_net_client.h"
#include "common.h"
#include <scd4x.h>

#define LOG_TAG "main"

#define TIME_OFFSET 1777000000

extern "C" {
void app_main();
}

static void send_to_server(uint8_t *payload, unsigned int payload_size, const uint32_t device_id)
{
  uint8_t *output;
  unsigned int output_size;
  time_t now;

  time(&now);

  now -= TIME_OFFSET;

  psa_status_t rc = encrypt_payload_aes(KEY_SERVER, payload, payload_size, &output, &output_size, static_cast<uint32_t>(now), device_id);
  if (rc != PSA_SUCCESS)
  {
    ESP_LOGE(LOG_TAG, "Encrypt payload error %d step %d", rc, psa_error_step);
    return;
  }
  send_udp(output, output_size);
  free(output);
}

static esp_err_t scd_get(scd4x_result *result)
{
  scd4x_wake_up();
  esp_err_t rc = scd4x_start_measurement();
  if (rc != ESP_OK)
  {
    ESP_LOGE(LOG_TAG, "scd4x_start_measurement failed\n");
    return rc;
  }
  vTaskDelay(6000 / portTICK_PERIOD_MS);
  rc = scd4x_read_measurement(result);
  if (rc != ESP_OK)
  {
    ESP_LOGE(LOG_TAG, "scd4x_read_measurement failed\n");
    return rc;
  }
  rc = scd4x_power_down();
  if (rc != ESP_OK)
    ESP_LOGE(LOG_TAG, "scd4x_power_down failed\n");
  ESP_LOGI(LOG_TAG, "CO2: %d temperature: %d humidity: %d", result->co2, result->temperature, result->humidity);
  return ESP_OK;
}

void app_main(void)
{
  common_init();

  configure_led();

  esp_err_t err = i2c_master_init();
  if (err != ESP_OK)
  {
    ESP_LOGE(LOG_TAG, "i2c_master_init failed with error %d", err);
    set_led_red();
    while (true)
      vTaskDelay(1000 / portTICK_PERIOD_MS);
  }

  psa_status_t rc = crypto_init();
  if (rc != PSA_SUCCESS)
  {
    ESP_LOGE(LOG_TAG, "crypto_init failed with error %d error step %d", rc, psa_error_step);
    set_led_red();
    while (true)
      vTaskDelay(1000 / portTICK_PERIOD_MS);
  }

  set_led_white();

  ESP_ERROR_CHECK(read_main_configuration());
  set_wifi_configuration_from_main_configuration();

  const auto _ieee802154 = new Ieee802154({.channel = main_config.channel, .pan_id = main_config.pan_id}, [](Ieee802154::Message message) {
    ESP_LOGI(LOG_TAG, "Got Message");
    ESP_LOGI(LOG_TAG, " -- source MAC: 0x%llx", message.source_address);
    ESP_LOGI(LOG_TAG, " -- payload size: %d", message.payload_size);
    uint8_t *output;
    unsigned int output_size;
    uint32_t device_id;
    psa_status_t rc = decrypt_payload(KEY_PAYLOAD, message.source_address, message.payload, message.payload_size, &output, &output_size, &device_id);
    if (rc == PSA_SUCCESS)
    {
      ESP_LOGI(LOG_TAG, "Message size %d, device id %d", output_size, device_id);
      send_to_server(output, output_size, device_id);
      free(output);
    }
    else
      ESP_LOGE(LOG_TAG, "Message decrypt failed with code %d", rc);
  });
  uint64_t mac_address = _ieee802154->deviceMacAddress();
  ESP_LOGI(LOG_TAG, "This device IEEE802.15.4 MAC: 0x%llx", mac_address);

  ESP_ERROR_CHECK(common_nvs_init());

  esp_coex_wifi_i154_enable();
  wifi_init();
  if (wifi_connect())
  {
    set_led_blue();
    while (true)
      vTaskDelay(1000 / portTICK_PERIOD_MS);
  }
  set_led_yellow();
  obtain_time();
  led_off();
  net_client_init();

  _ieee802154->initialize(false);

  while (true)
  {
    TickType_t time1 = xTaskGetTickCount();

    scd4x_result result;
    err = scd_get(&result);
    if (err == ESP_OK)
      send_to_server(reinterpret_cast<uint8_t*>(&result), sizeof(result), DEVICE_ID);
    TickType_t time2 = xTaskGetTickCount();
    TickType_t ms = (time2 - time1) * portTICK_PERIOD_MS;
    if (ms < SEND_INTERVAL)
      vTaskDelay((SEND_INTERVAL - ms) / portTICK_PERIOD_MS);
  }
}

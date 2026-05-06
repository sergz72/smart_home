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
#include <events.h>
#include "ieee802154_ble.h"
#include "scd4x_functions.h"

#define TIME_OFFSET 1777000000

static const char *LOG_TAG = "main";
static bool use_wifi;

extern "C" {
void app_main();
}

static void send_to_server(const uint32_t device_id, const uint8_t *payload, unsigned int payload_size)
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

esp_err_t ble_callback(uint8_t *message, size_t message_length, uint8_t **response, size_t *response_length)
{
  return ESP_OK;
}

static int events_init()
{
  multi_heap_info_t info;
  heap_caps_get_info(&info, MALLOC_CAP_INTERNAL | MALLOC_CAP_8BIT);
  ESP_LOGI(LOG_TAG, "Heap free: %d bytes", info.total_free_bytes);
  ESP_LOGI(LOG_TAG, "Heap free minimum: %d bytes", info.minimum_free_bytes);
  ESP_LOGI(LOG_TAG, "Heap free largest block: %d bytes", info.largest_free_block);
  size_t queue_size = info.total_free_bytes * 3 / 4;
  if (queue_size > info.largest_free_block)
    queue_size = info.largest_free_block;
  ESP_LOGI(LOG_TAG, "Requesting queue size: %d bytes...", queue_size);
  int rc = init_events(queue_size);
  if (rc != 0)
  {
    ESP_LOGE(LOG_TAG, "Failed to initialize events queue: %d", rc);
    return rc;
  }
  return 0;
}

static int event_add(uint32_t device_id, const uint8_t *message, int message_length)
{
  if (use_wifi)
  {
    send_to_server(device_id, message, message_length);
    return 0;
  }
  return add_event(device_id, message, message_length);
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

  initialise_button();
  set_led_green();

  vTaskDelay(3000 / portTICK_PERIOD_MS);
  use_wifi = !button_is_pressed();

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
      event_add(device_id, output, static_cast<int>(output_size));
      free(output);
    }
    else
      ESP_LOGE(LOG_TAG, "Message decrypt failed with code %d", rc);
  });
  uint64_t mac_address = _ieee802154->deviceMacAddress();
  ESP_LOGI(LOG_TAG, "This device IEEE802.15.4 MAC: 0x%llx", mac_address);

  ESP_ERROR_CHECK(common_nvs_init());

  if (use_wifi)
  {
    esp_coex_wifi_i154_enable();
    wifi_init();

    if (events_init() != 0)
    {
      set_led_red();
      while (true)
        vTaskDelay(1000 / portTICK_PERIOD_MS);
    }
  }
  if (!use_wifi || wifi_connect())
  {
    set_led_blue();
    ble_init(ble_callback);
  }
  else
  {
    set_led_yellow();
    obtain_time();
    net_client_init();
  }

  led_off();

  _ieee802154->initialize(false);

  while (true)
  {
    TickType_t time1 = xTaskGetTickCount();

    scd4x_result result;
    err = scd_get(&result);
    if (err == ESP_OK)
    {
      uint8_t *output;
      int output_size;
      scd_event_convert(&result, &output, &output_size);
      event_add(DEVICE_ID, output, output_size);
      free(output);
    }
    TickType_t time2 = xTaskGetTickCount();
    TickType_t ms = (time2 - time1) * portTICK_PERIOD_MS;
    if (ms < SEND_INTERVAL)
      vTaskDelay((SEND_INTERVAL - ms) / portTICK_PERIOD_MS);
  }
}

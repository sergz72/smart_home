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

#define LOG_TAG "main"

// Shared
struct __attribute__((packed)) ApplicationMessage {
  double temperature;
};

extern "C" {
void app_main();
}

void app_main(void) {
  common_init();

  configure_led();

  crypto_init();

  set_led_white();

  ESP_ERROR_CHECK(read_main_configuration());
  set_wifi_configuration_from_main_configuration();

  const auto _ieee802154 = new Ieee802154({.channel = main_config.channel, .pan_id = main_config.pan_id}, [](Ieee802154::Message message) {
    ESP_LOGI(LOG_TAG, "Got Message");
    ESP_LOGI(LOG_TAG, " -- source MAC: 0x%llx", message.source_address);
    ESP_LOGI(LOG_TAG, " -- destination MAC: 0x%llx", message.destination_address);
    ESP_LOGI(LOG_TAG, " -- payload size: %d", message.payload_size);
    uint8_t *output;
    unsigned int output_size;
    uint32_t device_id;
    psa_status_t rc = decrypt_payload(message.source_address, message.payload, message.payload_size, &output, &output_size, &device_id);
    if (rc == PSA_SUCCESS)
    {
      auto *app = reinterpret_cast<ApplicationMessage*>(output);
      ESP_LOGI(LOG_TAG, "Output size %d, device id %d temperature: %f", output_size, device_id, app->temperature);
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

  _ieee802154->initialize(false);

  while (true) {
    vTaskDelay(500 / portTICK_PERIOD_MS);
    fflush(stdout);
  }
}

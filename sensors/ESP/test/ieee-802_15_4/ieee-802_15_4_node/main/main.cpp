#include <Ieee802154.h>
#include <cstring>
#include <driver/gpio.h>
#include <esp_log.h>
#include <freertos/FreeRTOS.h>
#include <freertos/task.h>
#include "led.h"
#include "common_functions.h"

#define LOG_TAG "node"

// Shared
struct __attribute__((packed)) ApplicationMessage {
  double temperature;
};

ApplicationMessage message = {
  .temperature = 22.5,
};

void transmitTask(void *pvParameters) {
  auto *_ieee802154 = static_cast<Ieee802154*>(pvParameters);
  while (true) {
    uint8_t *output;
    unsigned int output_size;
    esp_err_t rc = encrypt_payload(reinterpret_cast<uint8_t*>(&message), sizeof(ApplicationMessage), &output, &output_size);
    if (rc != ESP_OK)
    {
      ESP_LOGE(LOG_TAG, "Encryption error %d", rc);
      vTaskDelay(500 / portTICK_PERIOD_MS);
      continue;
    }
    if (_ieee802154->transmit(main_config.host_mac_address, output, output_size))
    {
      increment_packet_counter();
      ESP_LOGI(LOG_TAG, "Transmit OK");
    }
    else
      ESP_LOGE(LOG_TAG, "Transmit error");
    vTaskDelay(2000 / portTICK_PERIOD_MS);
  }
}

extern "C" {
void app_main();
}

void app_main(void) {
  common_init();

  configure_led();

  crypto_init();

  set_led_white();

  ESP_ERROR_CHECK(read_main_configuration());
  const auto _ieee802154 = new Ieee802154({.channel = main_config.channel, .pan_id = main_config.pan_id});
  ESP_LOGI(LOG_TAG, "Channel: %d, PAN ID: 0x%04x", main_config.channel, main_config.pan_id);

  _ieee802154->initialize();
  ESP_LOGI(LOG_TAG, "This device IEEE802.15.4 MAC: 0x%llx", _ieee802154->deviceMacAddress());

  xTaskCreate(transmitTask, "transmitTask", 8192, _ieee802154, 15, nullptr);

  while (true) {
    vTaskDelay(500 / portTICK_PERIOD_MS);
    fflush(stdout);
  }
}

#include <Ieee802154.h>
#include <driver/gpio.h>
#include <esp_log.h>
#include <freertos/FreeRTOS.h>
#include <freertos/task.h>
#include "led.h"
#include "common_functions.h"
#include "esp_check.h"
#include "psa_crypto.h"
#include "esp_sleep.h"
//#include "esp_pm.h"
//#include "esp_private/esp_clk.h"

#define LOG_TAG "node"
#define TIMER_WAKEUP_TIME_US 5000000

//static esp_pm_lock_handle_t s_cli_pm_lock = nullptr;

// Shared
struct __attribute__((packed)) ApplicationMessage {
  double temperature;
};

ApplicationMessage message = {
  .temperature = 22.5,
};

esp_err_t register_timer_wakeup()
{
  ESP_RETURN_ON_ERROR(esp_sleep_enable_timer_wakeup(TIMER_WAKEUP_TIME_US), LOG_TAG, "Configure timer as wakeup source failed");
  ESP_LOGI(LOG_TAG, "timer wakeup source is ready");
  return ESP_OK;
}

/*static esp_err_t ot_power_save_init()
{
  int cur_cpu_freq_mhz = CONFIG_ESP_DEFAULT_CPU_FREQ_MHZ;

  esp_pm_config_t pm_config = {
    .max_freq_mhz = cur_cpu_freq_mhz,
    .min_freq_mhz = esp_clk_xtal_freq() / 1000000,
    .light_sleep_enable = false
  };

  return esp_pm_configure(&pm_config);
}

static esp_err_t sleep_device_init()
{
  esp_err_t ret = ESP_OK;

  ret = esp_pm_lock_create(ESP_PM_CPU_FREQ_MAX, 0, "otcli", &s_cli_pm_lock);
  if (ret == ESP_OK) {
    esp_pm_lock_acquire(s_cli_pm_lock);
    ESP_LOGI(LOG_TAG, "Successfully created CLI pm lock");
  } else {
    if (s_cli_pm_lock != nullptr) {
      esp_pm_lock_delete(s_cli_pm_lock);
      s_cli_pm_lock = nullptr;
    }
    ESP_LOGI(LOG_TAG, " Failed to create CLI pm lock");
  }
  return ret;
}*/

void transmitTask(Ieee802154 *_ieee802154) {
  while (true) {
    _ieee802154->teardown();
    register_timer_wakeup();
    esp_light_sleep_start();
    _ieee802154->initialize();
    uint8_t *output;
    unsigned int output_size;
    psa_status_t rc = encrypt_payload_aes(KEY_PAYLOAD, reinterpret_cast<uint8_t*>(&message), sizeof(ApplicationMessage), &output, &output_size);
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
    //vTaskDelay(2000 / portTICK_PERIOD_MS);
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
  ESP_LOGI(LOG_TAG, "This device IEEE802.15.4 MAC: 0x%llx", _ieee802154->deviceMacAddress());

  //ot_power_save_init();
  //sleep_device_init();
  transmitTask(_ieee802154);
}

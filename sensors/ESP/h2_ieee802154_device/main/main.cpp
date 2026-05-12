#include <Ieee802154.h>
#include <driver/gpio.h>
#include <esp_log.h>
#include <freertos/FreeRTOS.h>
#include <freertos/task.h>
#include "common.h"
#include "led.h"
#include "common_functions.h"
#include "esp_check.h"
#include "psa_crypto.h"
#include "esp_sleep.h"
#include "hal.h"
#include "scd4x_functions.h"
//#include "esp_pm.h"
//#include "esp_private/esp_clk.h"

#define LOG_TAG "node"

//static esp_pm_lock_handle_t s_cli_pm_lock = nullptr;
static uint32_t packet_counter;

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
    register_timer_wakeup(SEND_INTERVAL * 1000);
    esp_light_sleep_start();
    //vTaskDelay(6000 / portTICK_PERIOD_MS);
    scd4x_result result;
    esp_err_t err = scd_get(&result, true);
    if (err == ESP_OK)
    {
      _ieee802154->initialize(false);
      scd_event_convert(&result);
      uint8_t *output;
      unsigned int output_size;
      psa_status_t rc = encrypt_payload_aes(KEY_PAYLOAD, scd_event, sizeof(scd_event), &output, &output_size, packet_counter, 0);
      if (rc != ESP_OK)
      {
        ESP_LOGE(LOG_TAG, "Encryption error %d", rc);
        vTaskDelay(500 / portTICK_PERIOD_MS);
        continue;
      }
      if (_ieee802154->transmit(main_config.host_mac_address, output, output_size))
      {
        packet_counter++;
        ESP_LOGI(LOG_TAG, "Transmit OK");
      }
      else
        ESP_LOGE(LOG_TAG, "Transmit error");
    }
  }
}

extern "C" {
void app_main();
}

void app_main(void) {
  packet_counter = 1;

  common_init();

  configure_led();

  set_led_green();

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

  ESP_ERROR_CHECK(common_nvs_init());

  ESP_ERROR_CHECK(read_main_configuration());
  const auto _ieee802154 = new Ieee802154({.channel = main_config.channel, .pan_id = main_config.pan_id});
  ESP_LOGI(LOG_TAG, "Channel: %d, PAN ID: 0x%04x", main_config.channel, main_config.pan_id);
  ESP_LOGI(LOG_TAG, "This device IEEE802.15.4 MAC: 0x%llx", _ieee802154->deviceMacAddress());

  led_off();

  //_ieee802154->initialize(false);

  //ot_power_save_init();
  //sleep_device_init();
  transmitTask(_ieee802154);
}

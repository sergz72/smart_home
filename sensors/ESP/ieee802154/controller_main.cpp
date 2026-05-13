#include <Ieee802154.h>
#include <driver/gpio.h>
#include <esp_log.h>
#include <freertos/FreeRTOS.h>
#include <freertos/task.h>
#include <esp_coexist.h>
#include "common.h"
#include "led.h"
#ifndef NO_WIFI
#include "wifi.h"
#include "ieee802154_net_client.h"
#endif
#include "common_functions.h"
#include "psa_crypto.h"
#include "hal.h"
#include "ntp.h"
#include <scd4x.h>
#include <events.h>
#include "ieee802154_ble.h"
#include "scd4x_functions.h"
//#include "esp_pm.h"
//#include "esp_private/esp_clk.h"
//#include "esp_sleep.h"

#define TIME_OFFSET 1777000000

static const char *LOG_TAG = "main";
#ifndef NO_WIFI
static bool use_wifi;
#endif

static uint64_t conn_timestamps[CONFIG_BT_NIMBLE_MAX_CONNECTIONS + 1];

extern "C" {
void app_main();
}

#ifndef NO_WIFI
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
#endif

static int event_add(uint32_t device_id, const uint8_t *message, int message_length)
{
#ifndef NO_WIFI
  if (use_wifi)
  {
    send_to_server(device_id, message, message_length);
    return 0;
  }
#endif
  return add_event(device_id, message, message_length);
}

static void ble_write(const uint16_t conn_handle, const uint64_t timestamp)
{
  conn_timestamps[conn_handle] = timestamp;
}

static uint16_t ble_read(const uint16_t conn_handle, uint8_t **data)
{
  events_result_t result;
  int num_events = get_events_from(conn_handle, conn_timestamps[conn_handle], &result);
  if (num_events == 0)
    return 0;
  *data = reinterpret_cast<uint8_t*>(result.start);
  conn_timestamps[conn_handle] = (result.start[num_events - 1].timestampAndSensorId >> 8) + 1;
  return num_events * sizeof(sensor_event_with_time_t);
}

static void ble_connect(const uint16_t conn_handle)
{
  conn_timestamps[conn_handle] = 0;
}

static void ble_disconnect(const uint16_t conn_handle)
{
  conn_timestamps[conn_handle] = 0;
}

/*static esp_err_t power_save_init()
{
  int cur_cpu_freq_mhz = CONFIG_ESP_DEFAULT_CPU_FREQ_MHZ;

  esp_pm_config_t pm_config = {
    .max_freq_mhz = cur_cpu_freq_mhz,
    .min_freq_mhz = esp_clk_xtal_freq() / 1000000,
    .light_sleep_enable = true
  };

  return esp_pm_configure(&pm_config);
}*/

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

#ifndef NO_WIFI
  vTaskDelay(3000 / portTICK_PERIOD_MS);
  use_wifi = !button_is_pressed();
#else
  err = set_time_from_rtc();
  if (err != ESP_OK)
  {
    ESP_LOGE(LOG_TAG, "set_time_from_rtc failed with error %d", err);
    set_led_red();
    while (true)
      vTaskDelay(1000 / portTICK_PERIOD_MS);
  }
#endif

  set_led_white();
  ESP_ERROR_CHECK(read_main_configuration());
#ifndef NO_WIFI
  set_wifi_configuration_from_main_configuration();
#endif

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
  ESP_LOGI(LOG_TAG, "Channel: %d, PAN ID: 0x%04x", main_config.channel, main_config.pan_id);
  ESP_LOGI(LOG_TAG, "This device IEEE802.15.4 MAC: 0x%llx", mac_address);

  ESP_ERROR_CHECK(common_nvs_init());

  /*err = power_save_init();
  if (err != ESP_OK)
  {
    ESP_LOGE(LOG_TAG, "power_save_init failed with error %d", err);
    set_led_red();
    while (true)
      vTaskDelay(1000 / portTICK_PERIOD_MS);
  }
  esp_sleep_cpu_retention_init();*/

#ifndef NO_WIFI
  if (use_wifi)
  {
    esp_coex_wifi_i154_enable();
    wifi_init();
    init_events();
  }
  if (!use_wifi || wifi_connect())
  {
    err = esp_wifi_stop();
    if (err != ESP_OK)
      ESP_LOGE(LOG_TAG, "esp_wifi_stop failed with error %d", err);
    err = esp_wifi_deinit();
    if (err != ESP_OK)
      ESP_LOGE(LOG_TAG, "esp_wifi_deinit failed with error %d", err);
#endif
    err = ble_init(ble_write, ble_read, ble_connect, ble_disconnect);
    if (err != ESP_OK)
    {
      ESP_LOGE(LOG_TAG, "ble_init failed with error %d", err);
      set_led_red();
      while (true)
        vTaskDelay(1000 / portTICK_PERIOD_MS);
    }
    set_led_blue();
#ifndef NO_WIFI
  }
  else
  {
    set_led_yellow();
    obtain_time();
    net_client_init();
  }
#endif

  led_off();

  _ieee802154->initialize(false);

  while (true)
  {
    TickType_t time1 = xTaskGetTickCount();

    scd4x_result result;
    err = scd_get(&result, false);
    if (err == ESP_OK)
    {
      scd_event_convert(&result);
      event_add(DEVICE_ID, scd_event, sizeof(scd_event));
    }
    TickType_t time2 = xTaskGetTickCount();
    TickType_t ms = (time2 - time1) * portTICK_PERIOD_MS;
    if (ms < SEND_INTERVAL)
      vTaskDelay((SEND_INTERVAL - ms) / portTICK_PERIOD_MS);
  }
}

#include "common_functions.h"
#include <esp_partition.h>
#include <nvs_flash.h>
#include <string.h>
#include "esp_log.h"
#include "esp_random.h"
#include "esp_sleep.h"
#include "led.h"
#include "wifi.h"

#define TAG "common_functions"

#define STORAGE_PARTITION_NAME "storage"
#define MAIN_CONFIG_MAGIC 0x33

main_config_t main_config;

void common_init(void)
{
  memset(&main_config, 0, sizeof(main_config));
}

void fill_random(void *buf, size_t len)
{
  esp_fill_random(buf, len);
}

esp_err_t read_main_configuration(void)
{
  // Find the partition labeled "storage"
  const esp_partition_t *partition = esp_partition_find_first(
      ESP_PARTITION_TYPE_DATA,
      ESP_PARTITION_SUBTYPE_DATA_UNDEFINED,
      STORAGE_PARTITION_NAME
  );

  if (partition == NULL)
    return ESP_ERR_NVS_PART_NOT_FOUND;

  const esp_err_t rc = esp_partition_read(partition, 0, &main_config, sizeof(main_config));
  if (rc != ESP_OK)
    return rc;
  for (int i = 0; i < DEVICE_MAPPINGS_SIZE; i++)
    main_config.mac_mappings[i].last_packet_number = 0;
  return ESP_OK;
}

void set_wifi_configuration_from_main_configuration(void)
{
  if (main_config.magic != MAIN_CONFIG_MAGIC)
    return;
  strncpy((char*)wifi_config.sta.ssid, (const char*)main_config.wifi_ssid, sizeof(main_config.wifi_ssid));
  strncpy((char*)wifi_config.sta.password, (const char*)main_config.wifi_password, sizeof(main_config.wifi_password));
}

esp_err_t common_nvs_init(void)
{
  // Initialize NVS
  esp_err_t rc = nvs_flash_init();
  if (rc == ESP_ERR_NVS_NO_FREE_PAGES || rc == ESP_ERR_NVS_NEW_VERSION_FOUND) {
    // NVS partition was truncated and needs to be erased
    // Retry nvs_flash_init
    nvs_flash_erase();
    rc = nvs_flash_init();
  }
  return rc;
}

void register_timer_wakeup(uint64_t time)
{
  esp_err_t err = esp_sleep_enable_timer_wakeup(time);
  if (err != ESP_OK)
  {
    ESP_LOGE(TAG, "esp_sleep_enable_timer_wakeup failed with error %d", err);
    set_led_red();
    while (true)
      vTaskDelay(1000 / portTICK_PERIOD_MS);
  }
}
